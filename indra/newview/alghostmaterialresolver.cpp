/**
 * @file alghostmaterialresolver.cpp
 * @brief Source-resolved material snapshots for Ghost Studio clone backends.
 * @note Preserves source-resolved TE/legacy/PBR channels across
 *       LLViewerObject bake refresh and geometry-material rebuild paths.
 *
 * UNBUILT — pending Claude compile/link/in-world verification.
 */

#include "llviewerprecompiledheaders.h"

#include "alghostmaterialresolver.h"

#include "alghostattachmentenumerator.h"
#include "llavatarappearancedefines.h"
#include "llfetchedgltfmaterial.h"
#include "lllocalgltfmaterials.h"
#include "llviewerobject.h"
#include "llvovolume.h"
#include "llvoavatar.h"

namespace
{
const LLFetchedGLTFMaterial* effective_pbr(const LLVOVolume* volume, U8 te)
{
    const LLTextureEntry* entry = volume ? volume->getTE(te) : nullptr;
    return entry
        ? dynamic_cast<const LLFetchedGLTFMaterial*>(
              entry->getGLTFRenderMaterial())
        : nullptr;
}

LLUUID texture_id(const LLViewerTexture* texture)
{
    return texture ? texture->getID() : LLUUID::null;
}

bool resolved_bake_ready(const LLUUID& semantic_id,
                         const LLViewerTexture* texture)
{
    if (semantic_id.isNull())
    {
        return true;
    }
    if (!texture)
    {
        return false;
    }
    if (!LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::
            isBakedImageId(semantic_id))
    {
        return true;
    }
    return texture->getID() != IMG_DEFAULT &&
           texture->getID() != semantic_id &&
           !texture->isMissingAsset();
}

void audit_volume(const LLVOVolume* volume,
                  S32& objects,
                  S32& faces,
                  S32& baked_faces,
                  S32& pbr_faces)
{
    if (!volume || volume->isDead())
    {
        return;
    }
    ++objects;
    faces += volume->getNumTEs();
    for (U8 te = 0; te < volume->getNumTEs(); ++te)
    {
        const LLTextureEntry* entry = volume->getTE(te);
        if (!entry)
        {
            continue;
        }
        if (LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::
                isBakedImageId(entry->getID()))
        {
            ++baked_faces;
        }
        if (effective_pbr(volume, te))
        {
            ++pbr_faces;
        }
    }
}
}

ALGhostResolvedMaterialObject ALGhostMaterialResolver::capture(
    const LLVOVolume* source)
{
    ALGhostResolvedMaterialObject snapshot;
    if (!source || source->isDead())
    {
        return snapshot;
    }

    snapshot.mSourceObjectId = source->getID();
    snapshot.mFaceCount = source->getNumTEs();
    snapshot.mFaces.reserve(snapshot.mFaceCount);

    if (const LLRenderMaterialParams* params =
            source->getRenderMaterialParams())
    {
        snapshot.mRenderMaterialParams.copy(*params);
        snapshot.mHasRenderMaterialParams = true;
    }

    for (U8 te = 0; te < snapshot.mFaceCount; ++te)
    {
        const LLTextureEntry* entry = source->getTE(te);
        if (!entry)
        {
            continue;
        }

        ALGhostResolvedMaterialFace face;
        face.mIndex = te;
        face.mSemantic = *entry;
        face.mRenderMaterialId = source->getRenderMaterialID(te);
        face.mDiffuse = source->getTEImage(te);
        face.mNormal = source->getTENormalMap(te);
        face.mSpecular = source->getTESpecularMap(te);
        face.mHasBakedSemantic =
            LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::
                isBakedImageId(entry->getID());
        face.mResolvedReady =
            resolved_bake_ready(entry->getID(), face.mDiffuse.get());
        if (const LLMaterial* legacy = entry->getMaterialParams().get())
        {
            face.mResolvedReady =
                face.mResolvedReady &&
                resolved_bake_ready(
                    legacy->getNormalID(), face.mNormal.get()) &&
                resolved_bake_ready(
                    legacy->getSpecularID(), face.mSpecular.get());
        }

        if (const LLGLTFMaterial* material_override =
                entry->getGLTFMaterialOverride())
        {
            face.mOverride = new LLGLTFMaterial(*material_override);
        }

        if (const LLFetchedGLTFMaterial* pbr = effective_pbr(source, te))
        {
            face.mHasPbr = true;
            const bool local_material =
                LLLocalGLTFMaterialMgr::instanceExists() &&
                LLLocalGLTFMaterialMgr::getInstance()->isLocal(
                    face.mRenderMaterialId);
            if (pbr->isFetching() ||
                (!pbr->isLoaded() && !local_material))
            {
                face.mResolvedReady = false;
            }
            face.mPbrSemanticIds = pbr->mTextureId;
            face.mBaseColor = pbr->mBaseColorTexture.get();
            face.mPbrNormal = pbr->mNormalTexture.get();
            face.mMetallicRoughness =
                pbr->mMetallicRoughnessTexture.get();
            face.mEmissive = pbr->mEmissiveTexture.get();
            face.mResolvedReady =
                face.mResolvedReady &&
                resolved_bake_ready(
                    face.mPbrSemanticIds[
                        LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR],
                    face.mBaseColor.get()) &&
                resolved_bake_ready(
                    face.mPbrSemanticIds[
                        LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL],
                    face.mPbrNormal.get()) &&
                resolved_bake_ready(
                    face.mPbrSemanticIds[
                        LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS],
                    face.mMetallicRoughness.get()) &&
                resolved_bake_ready(
                    face.mPbrSemanticIds[
                        LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE],
                    face.mEmissive.get());
        }
        else if (face.mRenderMaterialId.notNull())
        {
            face.mResolvedReady = false;
        }
        snapshot.mFaces.push_back(face);
    }
    return snapshot;
}

bool ALGhostMaterialResolver::apply(
    LLVOVolume* destination,
    const ALGhostResolvedMaterialObject& snapshot)
{
    if (!destination || destination->isDead() ||
        snapshot.mSourceObjectId.isNull() ||
        destination->getNumTEs() != snapshot.mFaceCount ||
        snapshot.mFaces.size() != snapshot.mFaceCount)
    {
        LL_WARNS("GhostStudio")
            << "GHOSTMATRESOLVE apply rejected source="
            << snapshot.mSourceObjectId
            << " destination="
            << (destination ? destination->getID() : LLUUID::null)
            << " src_faces=" << static_cast<S32>(snapshot.mFaceCount)
            << " dst_faces="
            << (destination ? destination->getNumTEs() : 0)
            << LL_ENDL;
        return false;
    }

    // This first landing has no source-aware re-promotion scheduler. Installing
    // a pending sidecar would make its mReady=false state permanent even if
    // the source textures complete later. Fail before the first destination
    // mutation so the existing linkset/entity transaction can roll back and
    // the caller can retry after source loading completes.
    for (const ALGhostResolvedMaterialFace& face : snapshot.mFaces)
    {
        if (!face.mResolvedReady)
        {
            LL_WARNS("GhostStudio")
                << "GHOSTMATRESOLVE PENDING source="
                << snapshot.mSourceObjectId
                << " destination=" << destination->getID()
                << " face=" << static_cast<S32>(face.mIndex)
                << " -- retry spawn after source materials finish loading"
                << LL_ENDL;
            return false;
        }
    }

    // Register every channel before setTE(). setTE() synchronously calls
    // updateTEMaterialTextures(); the destination needs to know it is pinned
    // before that call so a baked PBR material is made clone-private before its
    // fetched texture fields are written.
    for (const ALGhostResolvedMaterialFace& face : snapshot.mFaces)
    {
        destination->setGhostResolvedBakedTexture(
            face.mSemantic.getID(), face.mDiffuse.get());
        const LLMaterialPtr legacy =
            face.mSemantic.getMaterialParams();
        if (legacy.notNull())
        {
            destination->setGhostResolvedBakedTexture(
                legacy->getNormalID(), face.mNormal.get());
            destination->setGhostResolvedBakedTexture(
                legacy->getSpecularID(), face.mSpecular.get());
        }
        destination->setGhostResolvedBakedTexture(
            face.mPbrSemanticIds[
                LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR],
            face.mBaseColor.get());
        destination->setGhostResolvedBakedTexture(
            face.mPbrSemanticIds[
                LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL],
            face.mPbrNormal.get());
        destination->setGhostResolvedBakedTexture(
            face.mPbrSemanticIds[
                LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS],
            face.mMetallicRoughness.get());
        destination->setGhostResolvedBakedTexture(
            face.mPbrSemanticIds[
                LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE],
            face.mEmissive.get());
        destination->setGhostResolvedMaterialBinding(
            face.mIndex,
            face.mSemantic,
            face.mRenderMaterialId,
            face.mResolvedReady,
            face.mDiffuse.get(),
            face.mNormal.get(),
            face.mSpecular.get(),
            face.mBaseColor.get(),
            face.mPbrNormal.get(),
            face.mMetallicRoughness.get(),
            face.mEmissive.get());
    }

    for (const ALGhostResolvedMaterialFace& face : snapshot.mFaces)
    {
        destination->setTE(face.mIndex, face.mSemantic);
    }

    if (snapshot.mHasRenderMaterialParams)
    {
        destination->setParameterEntry(
            LLNetworkData::PARAMS_RENDER_MATERIAL,
            snapshot.mRenderMaterialParams,
            /*local_origin=*/false);
    }

    // Overrides are a distinct layer and belong after base material IDs.
    for (const ALGhostResolvedMaterialFace& face : snapshot.mFaces)
    {
        if (face.mOverride.notNull())
        {
            destination->setTEGLTFMaterialOverride(
                face.mIndex, face.mOverride.get());
        }
        // setParameterEntry(PARAMS_RENDER_MATERIAL) may have replaced/cleared
        // the effective render material. Re-establish clone-private BoM/PBR
        // state before the final channel resolution.
        const LLTextureEntry* destination_te =
            destination->getTE(face.mIndex);
        if (destination_te && destination_te->getGLTFMaterial())
        {
            destination->initRenderMaterial(face.mIndex);
        }
        destination->updateTEMaterialTextures(face.mIndex);
        destination->restoreGhostResolvedMaterialBinding(face.mIndex);
    }

    S32 baked_faces = 0;
    S32 pbr_faces = 0;
    LLUUID first_material;
    for (const ALGhostResolvedMaterialFace& face : snapshot.mFaces)
    {
        baked_faces += face.mHasBakedSemantic ? 1 : 0;
        pbr_faces += face.mHasPbr ? 1 : 0;
        if (first_material.isNull() &&
            face.mRenderMaterialId.notNull())
        {
            first_material = face.mRenderMaterialId;
        }
    }
    LL_INFOS("GhostStudio")
        << "GHOSTMATRESOLVE source=" << snapshot.mSourceObjectId
        << " clone=" << destination->getID()
        << " faces=" << snapshot.mFaces.size()
        << " baked_faces=" << baked_faces
        << " pbr_faces=" << pbr_faces
        << " render_params=" << snapshot.mHasRenderMaterialParams
        << LL_ENDL;
    LL_INFOS("GhostStudio")
        << "GHOSTMATCOPY mode=source_resolved"
        << " src_obj=" << snapshot.mSourceObjectId
        << " clone_obj=" << destination->getID()
        << " render_params=" << snapshot.mHasRenderMaterialParams
        << " pbr_faces=" << pbr_faces
        << " first_material=" << first_material
        << LL_ENDL;
    return true;
}

void ALGhostMaterialResolver::auditSource(LLVOAvatar* source,
                                           const char* backend,
                                           bool exclude_temporary)
{
    const ALGhostAttachmentPlan plan =
        ALGhostAttachmentEnumerator::enumerateWorldRoots(
            source,
            exclude_temporary
                ? ALGhostTempAttachmentPolicy::EXCLUDE
                : ALGhostTempAttachmentPolicy::INCLUDE);
    if (!plan.mValidSource)
    {
        return;
    }

    S32 objects = 0;
    S32 faces = 0;
    S32 baked_faces = 0;
    S32 pbr_faces = 0;
    for (const ALGhostAttachmentRoot& root_entry : plan.mRoots)
    {
        LLVOVolume* root =
            dynamic_cast<LLVOVolume*>(root_entry.mRoot.get());
        audit_volume(root, objects, faces, baked_faces, pbr_faces);
        if (!root)
        {
            continue;
        }
        for (LLViewerObject* child : root->getChildren())
        {
            audit_volume(dynamic_cast<LLVOVolume*>(child),
                         objects, faces, baked_faces, pbr_faces);
        }
    }

    LL_INFOS("GhostStudio")
        << "GHOSTMATPREFLIGHT backend="
        << (backend ? backend : "unknown")
        << " source=" << plan.mSourceId
        << " roots=" << plan.expectedRoots()
        << " objects=" << objects
        << " faces=" << faces
        << " baked_faces=" << baked_faces
        << " pbr_faces=" << pbr_faces
        << LL_ENDL;
}

ALGhostMaterialResolver::VerifyStatus
ALGhostMaterialResolver::verifyBindings(
    const LLVOVolume* source,
    const LLVOVolume* clone)
{
    if (!clone)
    {
        return VerifyStatus::FAIL;
    }

    const U8 count = clone->getNumTEs();
    const bool binding_count_mismatch =
        clone->getGhostResolvedMaterialBindingCount() != count;
    const bool source_face_divergence =
        source && source->getNumTEs() != count;
    S32 captured_semantic_mismatch = 0;
    S32 pins_missing = 0;
    S32 pinned_value_mismatch = 0;
    S32 pending_faces = 0;
    S32 source_semantic_divergence = 0;
    S32 source_legacy_divergence = 0;
    S32 source_pbr_divergence = 0;

    for (U8 te = 0; te < count; ++te)
    {
        if (!clone->hasGhostResolvedMaterialBinding(te))
        {
            ++pins_missing;
        }
        else
        {
            if (!clone->ghostResolvedMaterialSemanticMatches(te))
            {
                ++captured_semantic_mismatch;
            }
            if (clone->isGhostResolvedMaterialBindingPending(te))
            {
                ++pending_faces;
            }
            else if (!clone->ghostResolvedMaterialBindingMatches(te))
            {
                ++pinned_value_mismatch;
            }
        }
        if (!source || te >= source->getNumTEs())
        {
            continue;
        }

        const LLTextureEntry* src_te = source->getTE(te);
        const LLTextureEntry* dst_te = clone->getTE(te);
        if (!src_te || !dst_te || src_te->getID() != dst_te->getID())
        {
            ++source_semantic_divergence;
        }
        if (texture_id(source->getTEImage(te)) !=
                texture_id(clone->getTEImage(te)) ||
            texture_id(source->getTENormalMap(te)) !=
                texture_id(clone->getTENormalMap(te)) ||
            texture_id(source->getTESpecularMap(te)) !=
                texture_id(clone->getTESpecularMap(te)))
        {
            ++source_legacy_divergence;
        }

        const LLFetchedGLTFMaterial* src_pbr = effective_pbr(source, te);
        const LLFetchedGLTFMaterial* dst_pbr = effective_pbr(clone, te);
        if ((src_pbr != nullptr) != (dst_pbr != nullptr))
        {
            ++source_pbr_divergence;
        }
        else if (src_pbr && dst_pbr &&
                 (texture_id(src_pbr->mBaseColorTexture.get()) !=
                      texture_id(dst_pbr->mBaseColorTexture.get()) ||
                  texture_id(src_pbr->mNormalTexture.get()) !=
                      texture_id(dst_pbr->mNormalTexture.get()) ||
                  texture_id(src_pbr->mMetallicRoughnessTexture.get()) !=
                      texture_id(dst_pbr->mMetallicRoughnessTexture.get()) ||
                  texture_id(src_pbr->mEmissiveTexture.get()) !=
                      texture_id(dst_pbr->mEmissiveTexture.get())))
        {
            ++source_pbr_divergence;
        }
    }

    const bool fail =
        binding_count_mismatch ||
        captured_semantic_mismatch != 0 ||
        pins_missing != 0 ||
        pinned_value_mismatch != 0;
    const VerifyStatus status = fail
        ? VerifyStatus::FAIL
        : (pending_faces > 0 ? VerifyStatus::PENDING : VerifyStatus::PASS);
    LL_INFOS("GhostStudio")
        << "GHOSTMATVERIFY source="
        << (source ? source->getID() : LLUUID::null)
        << " clone=" << clone->getID()
        << " src_faces="
        << (source ? static_cast<S32>(source->getNumTEs()) : -1)
        << " clone_faces=" << static_cast<S32>(clone->getNumTEs())
        << " binding_count_mismatch=" << binding_count_mismatch
        << " semantic_mismatch=" << captured_semantic_mismatch
        << " pins_missing=" << pins_missing
        << " pinned_value_mismatch=" << pinned_value_mismatch
        << " pending_faces=" << pending_faces
        << " source_face_divergence=" << source_face_divergence
        << " source_semantic_divergence="
        << source_semantic_divergence
        << " source_legacy_channel_divergence="
        << source_legacy_divergence
        << " source_pbr_channel_divergence="
        << source_pbr_divergence
        << " verdict="
        << (status == VerifyStatus::PASS
                ? "PASS"
                : (status == VerifyStatus::PENDING ? "PENDING" : "FAIL"))
        << LL_ENDL;
    return status;
}
