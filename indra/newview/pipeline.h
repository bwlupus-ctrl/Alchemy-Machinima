/**
 * @file pipeline.h
 * @brief Rendering pipeline definitions
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_PIPELINE_H
#define LL_PIPELINE_H

#include "llcamera.h"
#include "llerror.h"
#include "lldrawpool.h"
#include "llspatialpartition.h"
#include "m4math.h"
#include "llpointer.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolmaterials.h"
#include "llgl.h"
#include "lldrawable.h"
#include "llrendertarget.h"
#include "llreflectionmapmanager.h"
#include "llheroprobemanager.h"

#include <stack>

class LLViewerTexture;
class LLFace;
class LLViewerObject;
class LLTextureEntry;
class LLCullResult;
class LLVOAvatar;
class LLVOPartGroup;
class LLGLSLShader;
class LLDrawPoolAlpha;
class LLSettingsSky;

typedef enum e_avatar_skinning_method
{
    SKIN_METHOD_SOFTWARE,
    SKIN_METHOD_VERTEX_PROGRAM
} EAvatarSkinningMethod;

bool compute_min_max(LLMatrix4& box, LLVector2& min, LLVector2& max); // Shouldn't be defined here!
bool LLRayAABB(const LLVector3 &center, const LLVector3 &size, const LLVector3& origin, const LLVector3& dir, LLVector3 &coord, F32 epsilon = 0);
bool setup_hud_matrices(); // use whole screen to render hud
bool setup_hud_matrices(const LLRect& screen_region); // specify portion of screen (in pixels) to render hud attachments from (for picking)

class LLPipeline
{
public:
    static constexpr U32 BLOOM_MAX_MIPS = 7;

    LLPipeline();
    ~LLPipeline();

    void destroyGL();
    void restoreGL();
    void requestResizeScreenTexture(); // set flag only, no work, safer for callbacks...
    void requestResizeShadowTexture(); // set flag only, no work, safer for callbacks...

    void resizeScreenTexture();
    void resizeShadowTexture();

    void releaseGLBuffers();
    void releaseLUTBuffers();
    void releaseScreenBuffers();
    void releaseShadowBuffers();

    void createGLBuffers();
    void createLUTBuffers();
    void setupGradingLUT();

    //allocate the largest screen buffer possible up to resX, resY
    //returns true if full size buffer allocated, false if some other size is allocated
    bool allocateScreenBuffer(U32 resX, U32 resY);

    typedef enum {
        FBO_SUCCESS_FULLRES = 0,
        FBO_SUCCESS_LOWRES,
        FBO_FAILURE
    } eFBOStatus;

private:
    //implementation of above, wrapped for easy error handling
    eFBOStatus doAllocateScreenBuffer(U32 resX, U32 resY);
public:

    //attempt to allocate screen buffers at resX, resY
    //returns true if allocation successful, false otherwise
    bool allocateScreenBufferInternal(U32 resX, U32 resY);
    bool allocateShadowBuffer(U32 resX, U32 resY);

    // rebuild all LLVOVolume render batches
    void rebuildDrawInfo();
    // Rebuild all terrain
    void rebuildTerrain();

    // Clear LLFace mVertexBuffer pointers
    void resetVertexBuffers(LLDrawable* drawable);

    // perform a profile of the given avatar
    // if profile_attachments is true, run a profile for each attachment
    void profileAvatar(LLVOAvatar* avatar, bool profile_attachments = false);

    // generate an impostor for the given avatar
    //  preview_avatar - if true, a preview window render is being performed
    //  for_profile - if true, a profile is being performed, do not update actual impostor
    //  specific_attachment - specific attachment to profile, or nullptr to profile entire avatar
    void generateImpostor(LLVOAvatar* avatar, bool preview_avatar = false, bool for_profile = false, LLViewerObject* specific_attachment = nullptr);

    void bindScreenToTexture();
    void renderFinalize();
    void copyScreenSpaceReflections(LLRenderTarget* src, LLRenderTarget* dst);
    void generateLuminance(LLRenderTarget* src, LLRenderTarget* dst);
    void generateExposure(LLRenderTarget* src, LLRenderTarget* dst, bool use_history = true);
    void colorCorrect(LLRenderTarget* src, LLRenderTarget* dst, bool tonemap, bool colorgrade);
    void generateGlow(LLRenderTarget* src);
    void generateBloomHDR(LLRenderTarget* src);
    void compositeBloomHDR(LLRenderTarget* scene);
    void applyCAS(LLRenderTarget* src, LLRenderTarget* dst);
    // [BDMerge G3.2] volumetric lighting (donor: Black Dragon)
    void renderVolumetric(LLRenderTarget* src, LLRenderTarget* dst);
    // [BDMerge G3.3] per-projector volumetric light cones: additive pass, one
    // fullscreen cone per shadow-casting projector slot, in place on target.
    void renderProjectorVolumetric(LLRenderTarget* target);
    // [BDMerge G3.3 Phase 3 item 4] Controlled feed of the current frame's half-res
    // shaft (mProjVolHalf) into the HDR bloom pyramid (bloomMip[0]) for a soft glow
    // halo. Runs after renderProjectorVolumetric leaves the shaft in mProjVolHalf,
    // gated by BDMergeProjectorVolumetricsBloomFeed (default 0 = off) so it never
    // reintroduces uncontrolled bloom leakage.
    void feedProjectorVolumetricBloom();

    // [BDMerge G3.3 Phase 2] Session-only per-projector art-direction flag.
    // A projector emits a volumetric shaft only when its object UUID has been
    // opted in this session via the right-click "Volumetric Shaft" toggle.
    // The set is NOT persisted - it is cleared on logout/relog (see
    // clearVolumetricShafts(), called from LLAppViewer::disconnectViewer) and
    // starts empty, so the effect is off per-projector by default even when the
    // BDMergeProjectorVolumetrics master gate is on. Mirrors ALDerenderList's
    // selection plumbing but purely in-memory.
    static void  toggleVolumetricShaft(const LLUUID& id);
    static bool  isVolumetricShaftEnabled(const LLUUID& id);
    static void  clearVolumetricShafts();

    // [BDMerge G3.3 Batch 3] Session-only per-projector "cast shadows" opt-OUT.
    // A projector whose object UUID is in this set still lights the scene but is
    // excluded from shadow-slot (mTargetShadowSpotLight[]) eligibility, so it
    // casts no shadow and frees its slot for other projectors. Default: NOT in
    // the set == casts shadows exactly as before (no behavior change until
    // toggled). NOT persisted - cleared on relog via clearVolumetricShafts()
    // (called from LLAppViewer::disconnectViewer). Mirrors the sVolumetricShaft-
    // Objects flag-set pattern. isProjectorShadowSuppressed() applies the same
    // own-ID + root-edit fallback the volumetric filter uses.
    static void  toggleProjectorCastShadows(const LLUUID& id);
    static bool  isProjectorNoShadow(const LLUUID& id);
    static bool  isProjectorShadowSuppressed(LLVOVolume* volume);
    void applyFXAA(LLRenderTarget* src, LLRenderTarget* dst);
    void generateSMAABuffers(LLRenderTarget* src);
    void applySMAA(LLRenderTarget* src, LLRenderTarget* dst);
    void resolveSMAAT2x(LLRenderTarget* src, LLRenderTarget* dst); // [BDMerge A5.8] SMAA T2x
    void renderDoF(LLRenderTarget* src, LLRenderTarget* dst);
    void copyRenderTarget(LLRenderTarget* src, LLRenderTarget* dst);
    void combineGlow(LLRenderTarget* src, LLRenderTarget* dst);
    void visualizeBuffers(LLRenderTarget* src, LLRenderTarget* dst, U32 bufferIndex);

    void init();
    void cleanup();
    bool isInit() { return mInitialized; };

    /// @brief Get a draw pool from pool type (POOL_SIMPLE, POOL_MEDIA) and texture.
    /// @return Draw pool, or NULL if not found.
    LLDrawPool *findPool(const U32 pool_type, LLViewerTexture *tex0 = NULL);

    /// @brief Get a draw pool for faces of the appropriate type and texture.  Create if necessary.
    /// @return Always returns a draw pool.
    LLDrawPool *getPool(const U32 pool_type, LLViewerTexture *tex0 = NULL);

    /// @brief Figures out draw pool type from texture entry. Creates pool if necessary.
    static LLDrawPool* getPoolFromTE(const LLTextureEntry* te, LLViewerTexture* te_image);
    static U32 getPoolTypeFromTE(const LLTextureEntry* te, LLViewerTexture* imagep);

    void         addPool(LLDrawPool *poolp);    // Only to be used by LLDrawPool classes for splitting pools!
    void         removePool( LLDrawPool* poolp );

    void         allocDrawable(LLViewerObject *obj);

    void         unlinkDrawable(LLDrawable*);

    static void removeMutedAVsLights(LLVOAvatar*);

    // Object related methods
    void        markVisible(LLDrawable *drawablep, LLCamera& camera);
    void        markOccluder(LLSpatialGroup* group);

    void        doOcclusion(LLCamera& camera);
    void        markNotCulled(LLSpatialGroup* group, LLCamera &camera);
    void        markMoved(LLDrawable *drawablep, bool damped_motion = false);
    void        markShift(LLDrawable *drawablep);
    void        markTextured(LLDrawable *drawablep);
    void        markGLRebuild(LLGLUpdate* glu);
    void        markRebuild(LLSpatialGroup* group);
    void        markRebuild(LLDrawable *drawablep, LLDrawable::EDrawableFlags flag = LLDrawable::REBUILD_ALL);
    void        markPartitionMove(LLDrawable* drawablep);
    void        markMeshDirty(LLSpatialGroup* group);

    //get the object between start and end that's closest to start.
    LLViewerObject* lineSegmentIntersectInWorld(const LLVector4a& start, const LLVector4a& end,
                                                bool pick_transparent,
                                                bool pick_rigged,
                                                bool pick_unselectable,
                                                bool pick_reflection_probe,
                                                S32* face_hit,                          // return the face hit
                                                LLVector4a* intersection = NULL,         // return the intersection point
                                                LLVector2* tex_coord = NULL,            // return the texture coordinates of the intersection point
                                                LLVector4a* normal = NULL,               // return the surface normal at the intersection point
                                                LLVector4a* tangent = NULL             // return the surface tangent at the intersection point
        );

    //get the closest particle to start between start and end, returns the LLVOPartGroup and particle index
    LLVOPartGroup* lineSegmentIntersectParticle(const LLVector4a& start, const LLVector4a& end, LLVector4a* intersection,
                                                        S32* face_hit);


    LLViewerObject* lineSegmentIntersectInHUD(const LLVector4a& start, const LLVector4a& end,
                                              bool pick_transparent,
                                              S32* face_hit,                          // return the face hit
                                              LLVector4a* intersection = NULL,         // return the intersection point
                                              LLVector2* tex_coord = NULL,            // return the texture coordinates of the intersection point
                                              LLVector4a* normal = NULL,               // return the surface normal at the intersection point
                                              LLVector4a* tangent = NULL             // return the surface tangent at the intersection point
        );

    // Something about these textures has changed.  Dirty them.
    void        dirtyPoolObjectTextures(const std::set<LLViewerFetchedTexture*>& textures);

    void        resetDrawOrders();

    U32         addObject(LLViewerObject *obj);

    void        enableShadows(const bool enable_shadows);
    void        releaseSpotShadowTargets();
    void        releaseSunShadowTargets();
    void        releaseSunShadowTarget(U32 index);

    bool        shadersLoaded();
    bool        canUseWindLightShaders() const;
    bool        canUseAntiAliasing() const;

    // phases
    void resetFrameStats();

    void updateMoveDampedAsync(LLDrawable* drawablep);
    void updateMoveNormalAsync(LLDrawable* drawablep);
    void updateMovedList(LLDrawable::drawable_vector_t& move_list);
    void updateMove();
    bool visibleObjectsInFrustum(LLCamera& camera);
    bool getVisibleExtents(LLCamera& camera, LLVector3 &min, LLVector3& max);
    bool getVisiblePointCloud(LLCamera& camera, LLVector3 &min, LLVector3& max, std::vector<LLVector3>& fp, LLVector3 light_dir = LLVector3(0,0,0));

    // Populate given LLCullResult with results of a frustum cull of the entire scene against the given LLCamera
    void updateCull(LLCamera& camera, LLCullResult& result);
    void createObjects(F32 max_dtime);
    void createObject(LLViewerObject* vobj);
    void processPartitionQ();
    void updateGeom(F32 max_dtime);
    void updateGL();
    void rebuildPriorityGroups();
    void rebuildGroups();
    void clearRebuildGroups();
    void clearRebuildDrawables();

    //calculate pixel area of given box from vantage point of given camera
    static F32 calcPixelArea(LLVector3 center, LLVector3 size, LLCamera& camera);
    static F32 calcPixelArea(const LLVector4a& center, const LLVector4a& size, LLCamera &camera);

    void stateSort(LLCamera& camera, LLCullResult& result);
    void stateSort(LLSpatialGroup* group, LLCamera& camera);
    void stateSort(LLSpatialBridge* bridge, LLCamera& camera, bool fov_changed = false);
    void stateSort(LLDrawable* drawablep, LLCamera& camera);
    void postSort(LLCamera& camera);

    void forAllVisibleDrawables(void (*func)(LLDrawable*));

    void renderObjects(U32 type, bool texture = true, bool batch_texture = false, bool rigged = false);
    void renderGLTFObjects(U32 type, bool texture = true, bool rigged = false);

    void renderAlphaObjects(bool rigged = false);
    void renderMaskedObjects(U32 type, bool texture = true, bool batch_texture = false, bool rigged = false);
    void renderFullbrightMaskedObjects(U32 type, bool texture = true, bool batch_texture = false, bool rigged = false);

    void renderGroups(LLRenderPass* pass, U32 type, bool texture);
    void renderRiggedGroups(LLRenderPass* pass, U32 type, bool texture);

    void grabReferences(LLCullResult& result);
    void clearReferences();

    //check references will assert that there are no references in sCullResult to the provided data
    void checkReferences(LLFace* face);
    void checkReferences(LLDrawable* drawable);
    void checkReferences(LLDrawInfo* draw_info);
    void checkReferences(LLSpatialGroup* group);

    void renderGeomDeferred(LLCamera& camera, bool do_occlusion = false);
    void renderGeomPostDeferred(LLCamera& camera);
    void renderGeomShadow(LLCamera& camera);
    // [BDMerge A5.4-1a] Velocity / motion-vector geometry pass. Re-rasterizes the
    // opaque scene into mVelocityMap (RG16F) writing per-pixel screen-space
    // velocity. renderVelocityDebug blits it to screen for validation.
    void renderGeomVelocity();
    void renderVelocityDebug(LLRenderTarget* dst);
    void bindLightFunc(LLGLSLShader& shader);

    // bind shadow maps
    // if setup is true, wil lset texture compare mode function and filtering options
    void bindShadowMaps(LLGLSLShader& shader);
    void bindDeferredShaderFast(LLGLSLShader& shader);
    void bindDeferredShader(LLGLSLShader& shader, LLRenderTarget* light_target = nullptr, LLRenderTarget* depth_target = nullptr);
    void setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep);
    // [BDMerge G3.3] side-effect-free variant of setupSpotLight for the
    // finalize-stage volumetric pass: uploads only geometry + cookie (NO
    // mTargetShadowSpotLight priority reshuffle - R1), shadow slot passed in.
    void setupSpotLightVolumetric(LLGLSLShader& shader, LLDrawable* drawablep, S32 slot);

    void unbindDeferredShader(LLGLSLShader& shader);

    // set env_mat parameter in given shader
    void setEnvMat(LLGLSLShader& shader);

    void bindReflectionProbes(LLGLSLShader& shader);
    void unbindReflectionProbes(LLGLSLShader& shader);

    void renderDeferredLighting();

    // apply atmospheric haze based on contents of color and depth buffer
    // should be called just before rendering water when camera is under water
    // and just before rendering alpha when camera is above water
    void doAtmospherics();

    // apply water haze based on contents of color and depth buffer
    // should be called just before rendering pre-water alpha objects
    void doWaterHaze();

    // Generate the water exclusion surface mask.
    void doWaterExclusionMask();

    void postDeferredGammaCorrect(LLRenderTarget* screen_target);

    void generateSunShadow(LLCamera& camera);
    LLRenderTarget* getSunShadowTarget(U32 i);
    LLRenderTarget* getSpotShadowTarget(U32 i);

    void renderHighlight(const LLViewerObject* obj, F32 fade);

    void renderShadow(const glm::mat4& view, const glm::mat4& proj, LLCamera& camera, LLCullResult& result, bool depth_clamp, bool do_cull = true);
    void renderSelectedFaces(const LLColor4& color);
    void renderHighlights();
    void renderDebug();
    void renderPhysicsDisplay();
    // [F4] DoF focus point crosshair overlay (donor: Firestorm I:\enve
    // indra/newview/pipeline.cpp LLPipeline::renderFocusPoint, FIRE-32023 /
    // FIRE-16728; BD/Alchemy merge campaign item F4). Draws in the UI pass so
    // it is excluded from snapshots the same way as the rest of the UI.
    void renderFocusPoint();
    // [F8] Rule-of-thirds / golden-ratio / diagonal composition guide overlay
    // (donor: Firestorm I:\enve indra/newview/pipeline.cpp
    // LLPipeline::renderSnapshotGuidesOverlay; campaign item F8). Unlike the
    // donor, this is not gated on the snapshot floater's capture-frame border
    // (FS FSSnapshotShowCaptureFrame / gPostSnapshotFrameProgram were not
    // ported - see F4/F8 campaign report); it simply overlays the full
    // viewport whenever RenderCompositionGuide is enabled. Also UI-pass only.
    void renderCompositionGuideOverlay();

    void rebuildPools(); // Rebuild pools

    void findReferences(LLDrawable *drawablep); // Find the lists which have references to this object
    bool verify();                      // Verify that all data in the pipeline is "correct"

    S32  getLightCount() const { return static_cast<S32>(mLights.size()); }

    void calcNearbyLights(LLCamera& camera);
    void setupHWLights();
    void setupAvatarLights(bool for_edit = false);
    void enableLights(U32 mask);
    void enableLightsDynamic();
    void enableLightsAvatar();
    void enableLightsPreview();
    void enableLightsAvatarEdit(const LLColor4& color);
    void enableLightsFullbright();
    void disableLights();

    void shiftObjects(const LLVector3 &offset);

    void setLight(LLDrawable *drawablep, bool is_light);

    bool hasRenderBatches(const U32 type) const;
    LLCullResult::drawinfo_iterator beginRenderMap(U32 type);
    LLCullResult::drawinfo_iterator endRenderMap(U32 type);
    LLCullResult::sg_iterator beginAlphaGroups();
    LLCullResult::sg_iterator endAlphaGroups();
    LLCullResult::sg_iterator beginRiggedAlphaGroups();
    LLCullResult::sg_iterator endRiggedAlphaGroups();

    void addTrianglesDrawn(S32 index_count);
    void recordTrianglesDrawn();

    bool hasRenderDebugFeatureMask(const U32 mask) const    { return bool(mRenderDebugFeatureMask & mask); }
    bool hasRenderDebugMask(const U64 mask) const           { return bool(mRenderDebugMask & mask); }
    void setAllRenderDebugFeatures() { mRenderDebugFeatureMask = 0xffffffff; }
    void clearAllRenderDebugFeatures() { mRenderDebugFeatureMask = 0x0; }
    void setAllRenderDebugDisplays() { mRenderDebugMask = 0xffffffffffffffff; }
    void clearAllRenderDebugDisplays() { mRenderDebugMask = 0x0; }

    bool hasRenderType(const U32 type) const;
    bool hasAnyRenderType(const U32 type, ...) const;

    static bool isWaterClip();

    void setRenderTypeMask(U32 type, ...);
    // This is equivalent to 'setRenderTypeMask'
    //void orRenderTypeMask(U32 type, ...);
    void andRenderTypeMask(U32 type, ...);
    void clearRenderTypeMask(U32 type, ...);
    void setAllRenderTypes();
    void clearAllRenderTypes();

    void pushRenderTypeMask();
    void popRenderTypeMask();

    void pushRenderDebugFeatureMask();
    void popRenderDebugFeatureMask();

    static void toggleRenderType(U32 type);

    // For UI control of render features
    static bool hasRenderTypeControl(U32 data);
    static void toggleRenderDebug(U64 data);
    static void toggleRenderDebugFeature(U32 data);
    static void toggleRenderTypeControl(U32 data);
    static bool toggleRenderTypeControlNegated(S32 data);
    static bool toggleRenderDebugControl(U64 data);
    static bool toggleRenderDebugFeatureControl(U32 data);
    static void setRenderDebugFeatureControl(U32 bit, bool value);

    static void setRenderParticleBeacons(bool val);
    static void toggleRenderParticleBeacons();
    static bool getRenderParticleBeacons();

    static void setRenderSoundBeacons(bool val);
    static void toggleRenderSoundBeacons();
    static bool getRenderSoundBeacons();

    static void setRenderMOAPBeacons(bool val);
    static void toggleRenderMOAPBeacons();
    static bool getRenderMOAPBeacons();

    static void setRenderPhysicalBeacons(bool val);
    static void toggleRenderPhysicalBeacons();
    static bool getRenderPhysicalBeacons();

    static void setRenderScriptedBeacons(bool val);
    static void toggleRenderScriptedBeacons();
    static bool getRenderScriptedBeacons();

    static void setRenderScriptedTouchBeacons(bool val);
    static void toggleRenderScriptedTouchBeacons();
    static bool getRenderScriptedTouchBeacons();

    static void setRenderBeacons(bool val);
    static void toggleRenderBeacons();
    static bool getRenderBeacons();

    static void setRenderHighlights(bool val);
    static void toggleRenderHighlights();
    static bool getRenderHighlights();
    static void setRenderHighlightTextureChannel(LLRender::eTexIndex channel); // sets which UV setup to display in highlight overlay

    static void updateRenderTransparentWater();
    static void refreshCachedSettings();

    void addDebugBlip(const LLVector3& position, const LLColor4& color);

    void hidePermanentObjects( std::vector<U32>& restoreList );
    void restorePermanentObjects( const std::vector<U32>& restoreList );
    void skipRenderingOfTerrain( bool flag );
    void hideObject( const LLUUID& id );
    void restoreHiddenObject( const LLUUID& id );
    void handleShadowDetailChanged();

    LLReflectionMapManager mReflectionMapManager;
    LLHeroProbeManager mHeroProbeManager;

private:
    void unloadShaders();
    void addToQuickLookup( LLDrawPool* new_poolp );
    void removeFromQuickLookup( LLDrawPool* poolp );
    bool updateDrawableGeom(LLDrawable* drawable);
    void assertInitializedDoError();
    bool assertInitialized() { const bool is_init = isInit(); if (!is_init) assertInitializedDoError(); return is_init; };
    void connectRefreshCachedSettingsSafe(const std::string name);
    void hideDrawable( LLDrawable *pDrawable );
    void unhideDrawable( LLDrawable *pDrawable );
    void skipRenderingShadows();
public:
    enum {GPU_CLASS_MAX = 3 };

    enum LLRenderTypeMask
    {
        // Following are pool types (some are also object types)
        RENDER_TYPE_SKY                         = LLDrawPool::POOL_SKY,
        RENDER_TYPE_WL_SKY                      = LLDrawPool::POOL_WL_SKY,
        RENDER_TYPE_TERRAIN                     = LLDrawPool::POOL_TERRAIN,
        RENDER_TYPE_SIMPLE                      = LLDrawPool::POOL_SIMPLE,
        RENDER_TYPE_GRASS                       = LLDrawPool::POOL_GRASS,
        RENDER_TYPE_ALPHA_MASK                  = LLDrawPool::POOL_ALPHA_MASK,
        RENDER_TYPE_FULLBRIGHT_ALPHA_MASK       = LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK,
        RENDER_TYPE_FULLBRIGHT                  = LLDrawPool::POOL_FULLBRIGHT,
        RENDER_TYPE_BUMP                        = LLDrawPool::POOL_BUMP,
        RENDER_TYPE_MATERIALS                   = LLDrawPool::POOL_MATERIALS,
        RENDER_TYPE_AVATAR                      = LLDrawPool::POOL_AVATAR,
        RENDER_TYPE_CONTROL_AV                  = LLDrawPool::POOL_CONTROL_AV, // Animesh
        RENDER_TYPE_TREE                        = LLDrawPool::POOL_TREE,
        RENDER_TYPE_WATEREXCLUSION              = LLDrawPool::POOL_WATEREXCLUSION,
        RENDER_TYPE_VOIDWATER                   = LLDrawPool::POOL_VOIDWATER,
        RENDER_TYPE_WATER                       = LLDrawPool::POOL_WATER,
        RENDER_TYPE_GLTF_PBR                    = LLDrawPool::POOL_GLTF_PBR,
        RENDER_TYPE_GLTF_PBR_ALPHA_MASK         = LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK,
        RENDER_TYPE_ALPHA                       = LLDrawPool::POOL_ALPHA,
        RENDER_TYPE_ALPHA_PRE_WATER             = LLDrawPool::POOL_ALPHA_PRE_WATER,
        RENDER_TYPE_ALPHA_POST_WATER            = LLDrawPool::POOL_ALPHA_POST_WATER,
        RENDER_TYPE_GLOW                        = LLDrawPool::POOL_GLOW,
        RENDER_TYPE_PASS_SIMPLE                 = LLRenderPass::PASS_SIMPLE,
        RENDER_TYPE_PASS_SIMPLE_RIGGED = LLRenderPass::PASS_SIMPLE_RIGGED,
        RENDER_TYPE_PASS_GRASS                  = LLRenderPass::PASS_GRASS,
        RENDER_TYPE_PASS_FULLBRIGHT             = LLRenderPass::PASS_FULLBRIGHT,
        RENDER_TYPE_PASS_FULLBRIGHT_RIGGED = LLRenderPass::PASS_FULLBRIGHT_RIGGED,
        RENDER_TYPE_PASS_INVISIBLE              = LLRenderPass::PASS_INVISIBLE,
        RENDER_TYPE_PASS_INVISIBLE_RIGGED = LLRenderPass::PASS_INVISIBLE_RIGGED,
        RENDER_TYPE_PASS_INVISI_SHINY           = LLRenderPass::PASS_INVISI_SHINY,
        RENDER_TYPE_PASS_INVISI_SHINY_RIGGED = LLRenderPass::PASS_INVISI_SHINY_RIGGED,
        RENDER_TYPE_PASS_FULLBRIGHT_SHINY       = LLRenderPass::PASS_FULLBRIGHT_SHINY,
        RENDER_TYPE_PASS_FULLBRIGHT_SHINY_RIGGED = LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED,
        RENDER_TYPE_PASS_SHINY                  = LLRenderPass::PASS_SHINY,
        RENDER_TYPE_PASS_SHINY_RIGGED = LLRenderPass::PASS_SHINY_RIGGED,
        RENDER_TYPE_PASS_BUMP                   = LLRenderPass::PASS_BUMP,
        RENDER_TYPE_PASS_BUMP_RIGGED = LLRenderPass::PASS_BUMP_RIGGED,
        RENDER_TYPE_PASS_POST_BUMP              = LLRenderPass::PASS_POST_BUMP,
        RENDER_TYPE_PASS_POST_BUMP_RIGGED = LLRenderPass::PASS_POST_BUMP_RIGGED,
        RENDER_TYPE_PASS_GLOW                   = LLRenderPass::PASS_GLOW,
        RENDER_TYPE_PASS_GLOW_RIGGED = LLRenderPass::PASS_GLOW_RIGGED,
        RENDER_TYPE_PASS_GLTF_GLOW = LLRenderPass::PASS_GLTF_GLOW,
        RENDER_TYPE_PASS_GLTF_GLOW_RIGGED = LLRenderPass::PASS_GLTF_GLOW_RIGGED,
        RENDER_TYPE_PASS_ALPHA                  = LLRenderPass::PASS_ALPHA,
        RENDER_TYPE_PASS_ALPHA_MASK             = LLRenderPass::PASS_ALPHA_MASK,
        RENDER_TYPE_PASS_ALPHA_MASK_RIGGED = LLRenderPass::PASS_ALPHA_MASK_RIGGED,
        RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK  = LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
        RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK_RIGGED = LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
        RENDER_TYPE_PASS_MATERIAL               = LLRenderPass::PASS_MATERIAL,
        RENDER_TYPE_PASS_MATERIAL_RIGGED = LLRenderPass::PASS_MATERIAL_RIGGED,
        RENDER_TYPE_PASS_MATERIAL_ALPHA         = LLRenderPass::PASS_MATERIAL_ALPHA,
        RENDER_TYPE_PASS_MATERIAL_ALPHA_RIGGED = LLRenderPass::PASS_MATERIAL_ALPHA_RIGGED,
        RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK    = LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
        RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK_RIGGED = LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED,
        RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE= LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
        RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED = LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
        RENDER_TYPE_PASS_SPECMAP                = LLRenderPass::PASS_SPECMAP,
        RENDER_TYPE_PASS_SPECMAP_RIGGED = LLRenderPass::PASS_SPECMAP_RIGGED,
        RENDER_TYPE_PASS_SPECMAP_BLEND          = LLRenderPass::PASS_SPECMAP_BLEND,
        RENDER_TYPE_PASS_SPECMAP_BLEND_RIGGED = LLRenderPass::PASS_SPECMAP_BLEND_RIGGED,
        RENDER_TYPE_PASS_SPECMAP_MASK           = LLRenderPass::PASS_SPECMAP_MASK,
        RENDER_TYPE_PASS_SPECMAP_MASK_RIGGED = LLRenderPass::PASS_SPECMAP_MASK_RIGGED,
        RENDER_TYPE_PASS_SPECMAP_EMISSIVE       = LLRenderPass::PASS_SPECMAP_EMISSIVE,
        RENDER_TYPE_PASS_SPECMAP_EMISSIVE_RIGGED = LLRenderPass::PASS_SPECMAP_EMISSIVE_RIGGED,
        RENDER_TYPE_PASS_NORMMAP                = LLRenderPass::PASS_NORMMAP,
        RENDER_TYPE_PASS_NORMMAP_RIGGED = LLRenderPass::PASS_NORMMAP_RIGGED,
        RENDER_TYPE_PASS_NORMMAP_BLEND          = LLRenderPass::PASS_NORMMAP_BLEND,
        RENDER_TYPE_PASS_NORMMAP_BLEND_RIGGED = LLRenderPass::PASS_NORMMAP_BLEND_RIGGED,
        RENDER_TYPE_PASS_NORMMAP_MASK           = LLRenderPass::PASS_NORMMAP_MASK,
        RENDER_TYPE_PASS_NORMMAP_MASK_RIGGED = LLRenderPass::PASS_NORMMAP_MASK_RIGGED,
        RENDER_TYPE_PASS_NORMMAP_EMISSIVE       = LLRenderPass::PASS_NORMMAP_EMISSIVE,
        RENDER_TYPE_PASS_NORMMAP_EMISSIVE_RIGGED = LLRenderPass::PASS_NORMMAP_EMISSIVE_RIGGED,
        RENDER_TYPE_PASS_NORMSPEC               = LLRenderPass::PASS_NORMSPEC,
        RENDER_TYPE_PASS_NORMSPEC_RIGGED = LLRenderPass::PASS_NORMSPEC_RIGGED,
        RENDER_TYPE_PASS_NORMSPEC_BLEND         = LLRenderPass::PASS_NORMSPEC_BLEND,
        RENDER_TYPE_PASS_NORMSPEC_BLEND_RIGGED = LLRenderPass::PASS_NORMSPEC_BLEND_RIGGED,
        RENDER_TYPE_PASS_NORMSPEC_MASK          = LLRenderPass::PASS_NORMSPEC_MASK,
        RENDER_TYPE_PASS_NORMSPEC_MASK_RIGGED = LLRenderPass::PASS_NORMSPEC_MASK_RIGGED,
        RENDER_TYPE_PASS_NORMSPEC_EMISSIVE      = LLRenderPass::PASS_NORMSPEC_EMISSIVE,
        RENDER_TYPE_PASS_NORMSPEC_EMISSIVE_RIGGED = LLRenderPass::PASS_NORMSPEC_EMISSIVE_RIGGED,
        RENDER_TYPE_PASS_GLTF_PBR                 = LLRenderPass::PASS_GLTF_PBR,
        RENDER_TYPE_PASS_GLTF_PBR_RIGGED         = LLRenderPass::PASS_GLTF_PBR_RIGGED,
        RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK        = LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK,
        RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK_RIGGED = LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED,
        // Following are object types (only used in drawable mRenderType)
        RENDER_TYPE_HUD = LLRenderPass::NUM_RENDER_TYPES,
        RENDER_TYPE_VOLUME,
        RENDER_TYPE_PARTICLES,
        RENDER_TYPE_CLOUDS,
        RENDER_TYPE_HUD_PARTICLES,
        NUM_RENDER_TYPES,
        END_RENDER_TYPES = NUM_RENDER_TYPES
    };

    enum LLRenderDebugFeatureMask
    {
        RENDER_DEBUG_FEATURE_UI                 = 0x0001,
        RENDER_DEBUG_FEATURE_SELECTED           = 0x0002,
        RENDER_DEBUG_FEATURE_HIGHLIGHTED        = 0x0004,
        RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES   = 0x0008,
//      RENDER_DEBUG_FEATURE_HW_LIGHTING        = 0x0010,
        RENDER_DEBUG_FEATURE_FLEXIBLE           = 0x0010,
        RENDER_DEBUG_FEATURE_FOG                = 0x0020,
        RENDER_DEBUG_FEATURE_FR_INFO            = 0x0080,
        RENDER_DEBUG_FEATURE_FOOT_SHADOWS       = 0x0100,
    };

    enum LLRenderDebugMask: U64
    {
        RENDER_DEBUG_COMPOSITION        =  0x00000001,
        RENDER_DEBUG_VERIFY             =  0x00000002,
        RENDER_DEBUG_BBOXES             =  0x00000004,
        RENDER_DEBUG_OCTREE             =  0x00000008,
        RENDER_DEBUG_WIND_VECTORS       =  0x00000010,
        RENDER_DEBUG_OCCLUSION          =  0x00000020,
        RENDER_DEBUG_POINTS             =  0x00000040,
        RENDER_DEBUG_TEXTURE_PRIORITY   =  0x00000080,
        RENDER_DEBUG_TEXTURE_AREA       =  0x00000100,
        RENDER_DEBUG_FACE_AREA          =  0x00000200,
        RENDER_DEBUG_PARTICLES          =  0x00000400,
        RENDER_DEBUG_GLOW               =  0x00000800, // not used
        RENDER_DEBUG_TEXTURE_ANIM       =  0x00001000,
        RENDER_DEBUG_LIGHTS             =  0x00002000,
        RENDER_DEBUG_BATCH_SIZE         =  0x00004000,
        RENDER_DEBUG_ALPHA_BINS         =  0x00008000, // not used
        RENDER_DEBUG_RAYCAST            =  0x00010000,
        RENDER_DEBUG_AVATAR_DRAW_INFO   =  0x00020000,
        RENDER_DEBUG_SHADOW_FRUSTA      =  0x00040000,
        RENDER_DEBUG_SCULPTED           =  0x00080000,
        RENDER_DEBUG_AVATAR_VOLUME      =  0x00100000,
        RENDER_DEBUG_AVATAR_JOINTS      =  0x00200000,
        RENDER_DEBUG_AGENT_TARGET       =  0x00800000,
        RENDER_DEBUG_UPDATE_TYPE        =  0x01000000,
        RENDER_DEBUG_PHYSICS_SHAPES     =  0x02000000,
        RENDER_DEBUG_NORMALS            =  0x04000000,
        RENDER_DEBUG_LOD_INFO           =  0x08000000,
        RENDER_DEBUG_NODES              =  0x20000000,
        RENDER_DEBUG_TEXEL_DENSITY      =  0x40000000,
        RENDER_DEBUG_TRIANGLE_COUNT     =  0x80000000,
        RENDER_DEBUG_IMPOSTORS          = 0x100000000,
        RENDER_DEBUG_REFLECTION_PROBES  = 0x200000000,
        RENDER_DEBUG_PROBE_UPDATES      = 0x400000000,
    };

public:

    LLSpatialPartition* getSpatialPartition(LLViewerObject* vobj);

    void updateCamera(bool reset = false);

    LLVector3               mFlyCamPosition;
    LLQuaternion            mFlyCamRotation;

    bool                     mBackfaceCull;
    S32                      mMatrixOpCount;
    S32                      mTextureMatrixOps;
    S32                      mNumVisibleNodes;

    S32                      mDebugTextureUploadCost;
    S32                      mDebugSculptUploadCost;
    S32                      mDebugMeshUploadCost;

    S32                      mNumVisibleFaces;

    S32                     mPoissonOffset;

    static S32              sCompiles;

    static bool             sShowHUDAttachments;
    static bool             sForceOldBakedUpload; // If true will not use capabilities to upload baked textures.
    static S32              sUseOcclusion;  // 0 = no occlusion, 1 = read only, 2 = read/write
    static bool             sAutoMaskAlphaDeferred;
    static bool             sAutoMaskAlphaNonDeferred;
    static bool             sRenderTransparentWater;
    static bool             sBakeSunlight;
    static bool             sNoAlpha;
    static bool             sUseFarClip;
    static bool             sShadowRender;
    // [BDMerge A5.4-1a] true only while the velocity/motion-vector geometry pass
    // is executing (renderGeomVelocity). Lets pool code and shaders distinguish
    // the velocity pass from the normal render if needed.
    static bool             sVelocityRender;
    static bool             sDynamicLOD;
    static bool             sPickAvatar;
    static bool             sReflectionRender;
    static bool             sDistortionRender;
    static bool             sImpostorRender;
    static bool             sImpostorRenderAlphaDepthPass;
    static bool             sUnderWaterRender;
    static bool             sRenderGlow;
    static bool             sTextureBindTest;
    static bool             sRenderAttachedLights;
    static bool             sRenderAttachedParticles;
    static bool             sT2xJitterEnabled; // [BDMerge A5.8] gate ±0.25px SMAA T2x subpixel jitter
    static bool             sRenderDeferred;
    static bool             sReflectionProbesEnabled;
    static S32              sVisibleLightCount;
    static bool             sRenderingHUDs;
    static F32              sDistortionWaterClipPlaneMargin;
// [SL:KB] - Patch: Render-TextureToggle (Catznip-4.0)
    static bool             sRenderTextures;
// [/SL:KB]
// [RLVa:KB] - @setsphere
    static bool             sUseDepthTexture;
// [/RLVa:KB]

    static LLTrace::EventStatHandle<S64> sStatBatchSize;

    class RenderTargetPack
    {
    public:
        U32                     width = 0;
        U32                     height = 0;

        //screen texture
        LLRenderTarget          screen;
        LLRenderTarget          deferredScreen;
        LLRenderTarget          deferredLight;

        // tonemapped and gamma corrected render ready for post
        LLRenderTarget          postPingMap;
        LLRenderTarget          postPongMap;

        //sun shadow map
        LLRenderTarget          shadow[4];

        // HDR bloom pyramid (RGB = bloom, A = halation intensity).
        // mBloomMip[0] is full-res extract; subsequent levels are halved.
        LLRenderTarget              bloomMip[BLOOM_MAX_MIPS];
        U32                         bloomMipCount = 0;
    };

    // main full resoltuion render target
    RenderTargetPack mMainRT;

    // auxillary 512x512 render target pack
    // used by reflection probes and dynamic texture bakes
    RenderTargetPack mAuxillaryRT;

    // Auxillary render target pack scaled to the hero probe's per-face size.
    RenderTargetPack mHeroProbeRT;

    // currently used render target pack
    RenderTargetPack* mRT;

    // [BDMerge NSpot] compile-time ceiling for projector shadows; runtime
    // count is BDMergeMaxSpotShadows (2 = stock)
    static constexpr U32    MAX_SPOT_SHADOWS = 6;
    static constexpr U32    MAX_SHADOW_MATS = 4 + MAX_SPOT_SHADOWS;
    LLRenderTarget          mSpotShadow[MAX_SPOT_SHADOWS];

    LLRenderTarget          mPbrBrdfLut;
    LLRenderTarget          mWaterExclusionMask;

    // copy of the color/depth buffer just before gamma correction
    // for use by SSR
    LLRenderTarget          mSceneMap;

    // [BDMerge G3.3 Phase 1 item 3] half-resolution scratch target the projector
    // volumetric cones march into (RGBA16F, rgb = additive shaft). Depth-aware
    // bilateral upsample composites it onto mRT->screen. Allocated on demand in
    // renderProjectorVolumetric, released in releaseGLBuffers/destroyGL.
    LLRenderTarget          mProjVolHalf;
    // [BDMerge G3.3 Phase 3 item 4] true iff the half-res march this frame produced
    // a real shaft in mProjVolHalf (half-res path + at least one cone drawn), so the
    // bloom feed knows the texture is current and safe to sample.
    bool                    mProjVolHalfValid = false;

    // [BDMerge G3.3 Batch 1 A] Temporal reprojection accumulation. Two half-res
    // RGBA16F history targets (ping-pong): rgb = accumulated shaft, a = view-space
    // depth stored for the disocclusion test. The temporal resolve reads the
    // previous slot + the freshly-marched mProjVolHalf and writes the blended
    // result into the current slot, which then feeds the upsample and bloom passes.
    // Allocated/cleared with mProjVolHalf, released in releaseGLBuffers.
    LLRenderTarget          mProjVolHistory[2];
    U32                     mProjVolHistoryIdx = 0;   // current write slot
    bool                    mProjVolHistoryValid = false; // false => no valid prev
    F32                     mProjVolPrevViewProj[16]; // world->clip of the last resolve
    // The shaft texture the upsample/bloom passes should sample this frame: the
    // temporal-resolved history slot when temporal is on, else &mProjVolHalf.
    LLRenderTarget*         mProjVolShaftSrc = nullptr;

    // exposure map for getting average color in scene
    LLRenderTarget          mLuminanceMap;
    LLRenderTarget          mExposureMap;
    LLRenderTarget          mLastExposure;

    // FXAA helper target
    LLRenderTarget          mFXAAMap;
    LLRenderTarget          mSMAABlendBuffer;
    // [BDMerge A5.8] SMAA T2x temporal path. mSMAAHistory holds the previous
    // frame's SMAA output; mSMAAFrameIndex alternates 0/1 to drive the ±0.25px
    // camera jitter and the blend-weights subsample offset. This block is in a
    // public section (see 'public:' above) so LLViewerCamera can read the frame
    // index when building the jittered projection.
    LLRenderTarget          mSMAAHistory;
    U32                     mSMAAFrameIndex = 0;

    // [BDMerge A5.4-1a] Screen-space velocity / motion-vector buffer (GL_RG16F).
    // Shares the deferred screen's depth so the velocity pass depth-tests against
    // the already-rendered opaque scene. Allocated only when BDMergeVelocityBuffer
    // is on (Phase 2 will also allocate it whenever SMAA T2x is active).
    LLRenderTarget          mVelocityMap;
    // Un-jittered current projection captured at camera setup (llviewercamera),
    // uploaded to the velocity programs so the SMAA T2x jitter does not leak into
    // motion vectors. Public so LLViewerCamera can write it.
    F32                     mVelocityProjMat[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    // render ui to buffer target
    LLRenderTarget          mUIScreen;

    // downres scratch space for GPU downscaling of textures
    LLRenderTarget          mDownResMap;

    // 2k bom scratch target
    LLRenderTarget          mBakeMap;

    LLCullResult            mSky;
    LLCullResult            mReflectedObjects;
    LLCullResult            mRefractedObjects;

    //utility buffers for rendering post effects
    LLPointer<LLVertexBuffer> mDeferredVB;

    // a single triangle that covers the whole screen
    LLPointer<LLVertexBuffer> mScreenTriangleVB;

    //utility buffer for rendering cubes, 8 vertices are corners of a cube [-1, 1]
    LLPointer<LLVertexBuffer> mCubeVB;

    //list of currently bound reflection maps
    std::vector<LLReflectionMap*> mReflectionMaps;

    std::vector<LLVector3>  mShadowFrustPoints[4];
    LLVector4               mShadowError;
    LLVector4               mShadowFOV;
    LLVector3               mShadowFrustOrigin[4];
    LLCamera                mShadowCamera[8];
    LLVector3               mShadowExtents[4][2];
    // TODO : separate Sun Shadow and Spot Shadow matrices
    glm::mat4               mSunShadowMatrix[MAX_SHADOW_MATS];
    glm::mat4               mShadowModelview[MAX_SHADOW_MATS];
    glm::mat4               mShadowProjection[MAX_SHADOW_MATS];
    glm::mat4               mReflectionModelView;

    LLPointer<LLDrawable>   mShadowSpotLight[MAX_SPOT_SHADOWS];
    F32                     mSpotLightFade[MAX_SPOT_SHADOWS];
    LLPointer<LLDrawable>   mTargetShadowSpotLight[MAX_SPOT_SHADOWS];

    LLVector4               mSunClipPlanes;
    LLVector4               mSunOrthoClipPlanes;
    LLVector2               mScreenScale;

    //water distortion texture (refraction)
    LLRenderTarget              mWaterDis;

    static const U32 MAX_PREVIEW_WIDTH;

    //texture for making the glow
    LLRenderTarget              mGlow[3];

    //noise map
    U32                 mNoiseMap;
    U32                 mTrueNoiseMap;
    U32                 mLightFunc;

    //smaa
    U32                 mSMAAAreaMap = 0;
    U32                 mSMAASearchMap = 0;
    U32                 mSMAASampleMap = 0;

    LLColor4            mSunDiffuse;
    LLColor4            mMoonDiffuse;
    LLVector4           mSunDir;
    LLVector4           mMoonDir;
    bool                mNeedsShadowTargetClear;

    LLVector4           mTransformedSunDir;
    LLVector4           mTransformedMoonDir;

    F32                 mLensFlareSunVisibility = 0.f;

    bool                    mInitialized;
    bool                    mShadersLoaded;

protected:
    bool                    mRenderTypeEnabled[NUM_RENDER_TYPES];
    std::stack<std::string> mRenderTypeEnableStack;

    U32                     mRenderDebugFeatureMask;
    U64                     mRenderDebugMask;
    U64                     mOldRenderDebugMask;
    std::stack<U32>         mRenderDebugFeatureStack;

    /////////////////////////////////////////////
    //
    //
    LLDrawable::drawable_vector_t   mMovedList;
    LLDrawable::drawable_vector_t mMovedBridge;
    LLDrawable::drawable_vector_t   mShiftList;

    /////////////////////////////////////////////
    //
    //
    struct Light
    {
        Light(LLDrawable* ptr, F32 d, F32 f = 0.0f)
            : drawable(ptr),
              dist(d),
              fade(f)
        {}
        LLPointer<LLDrawable> drawable;
        F32 dist;
        F32 fade;
        struct compare
        {
            bool operator()(const Light& a, const Light& b) const
            {
                if ( a.dist < b.dist )
                    return true;
                else if ( a.dist > b.dist )
                    return false;
                else
                    return a.drawable < b.drawable;
            }
        };
    };
    typedef std::set< Light, Light::compare > light_set_t;

    LLDrawable::ordered_drawable_set_t  mLights;
    light_set_t                     mNearbyLights; // lights near camera
    LLColor4                        mHWLightColors[8];

    /////////////////////////////////////////////
    //
    // Different queues of drawables being processed.
    //
    LLDrawable::drawable_list_t     mBuildQ1; // priority
    LLSpatialGroup::sg_vector_t     mGroupQ1; //priority

    LLSpatialGroup::sg_vector_t     mGroupSaveQ1; // a place to save mGroupQ1 until it is safe to unref

    LLSpatialGroup::sg_vector_t     mMeshDirtyGroup; //groups that need rebuildMesh called

    LLDrawable::drawable_list_t     mPartitionQ; //drawables that need to update their spatial partition radius

    bool mGroupQ1Locked;

    bool mResetVertexBuffers; //if true, clear vertex buffers on next update

    LLViewerObject::vobj_list_t     mCreateQ;

    LLDrawable::drawable_set_t      mRetexturedList;

    class HighlightItem
    {
    public:
        const LLPointer<LLDrawable> mItem;
        mutable F32 mFade;

        HighlightItem(LLDrawable* item)
        : mItem(item), mFade(0)
        {
        }

        bool operator<(const HighlightItem& rhs) const
        {
            return mItem < rhs.mItem;
        }

        bool operator==(const HighlightItem& rhs) const
        {
            return mItem == rhs.mItem;
        }

        void incrFade(F32 val) const
        {
            mFade = llclamp(mFade+val, 0.f, 1.f);
        }
    };

    //////////////////////////////////////////////////
    //
    // Draw pools are responsible for storing all rendered data,
    // and performing the actual rendering of objects.
    //
    struct compare_pools
    {
        bool operator()(const LLDrawPool* a, const LLDrawPool* b) const
        {
            if (!a)
                return true;
            else if (!b)
                return false;
            else
            {
                S32 atype = a->getType();
                S32 btype = b->getType();
                if (atype < btype)
                    return true;
                else if (atype > btype)
                    return false;
                else
                    return a->getId() < b->getId();
            }
        }
    };
    typedef std::set<LLDrawPool*, compare_pools > pool_set_t;
    pool_set_t mPools;
    LLDrawPool* mLastRebuildPool;

    // For quick-lookups into mPools (mapped by texture pointer)
    std::map<uintptr_t, LLDrawPool*>    mTerrainPools;
    std::map<uintptr_t, LLDrawPool*>    mTreePools;
    LLDrawPoolAlpha*            mAlphaPoolPreWater = nullptr;
    LLDrawPoolAlpha*            mAlphaPoolPostWater = nullptr;
    LLDrawPool*                 mSkyPool = nullptr;
    LLDrawPool*                 mTerrainPool = nullptr;
    LLDrawPool*                 mWaterPool = nullptr;
    LLRenderPass*               mSimplePool = nullptr;
    LLRenderPass*               mGrassPool = nullptr;
    LLRenderPass*               mAlphaMaskPool = nullptr;
    LLRenderPass*               mFullbrightAlphaMaskPool = nullptr;
    LLRenderPass*               mFullbrightPool = nullptr;
    LLDrawPool*                 mGlowPool = nullptr;
    LLDrawPool*                 mBumpPool = nullptr;
    LLDrawPool*                 mMaterialsPool = nullptr;
    LLDrawPool*                 mWLSkyPool = nullptr;
    LLDrawPool*                 mPBROpaquePool = nullptr;
    LLDrawPool*                 mPBRAlphaMaskPool = nullptr;
    LLDrawPool*                 mWaterExclusionPool      = nullptr;

    // Note: no need to keep an quick-lookup to avatar pools, since there's only one per avatar

    // Color grading lookup texture and size
    U32       mCGLut{};
    LLVector4 mCGLutSize{};

public:
    std::vector<LLFace*>        mHighlightFaces;    // highlight faces on physical objects
protected:
    std::vector<LLFace*>        mSelectedFaces;

    class DebugBlip
    {
    public:
        LLColor4 mColor;
        LLVector3 mPosition;
        F32 mAge;

        DebugBlip(const LLVector3& position, const LLColor4& color)
            : mColor(color), mPosition(position), mAge(0.f)
        { }
    };

    std::list<DebugBlip> mDebugBlips;

    LLPointer<LLViewerFetchedTexture>   mFaceSelectImagep;

    U32                     mLightMask;
    U32                     mLightMovingMask;

    static bool             sRenderPhysicalBeacons;
    static bool             sRenderMOAPBeacons;
    static bool             sRenderScriptedTouchBeacons;
    static bool             sRenderScriptedBeacons;
    static bool             sRenderParticleBeacons;
    static bool             sRenderSoundBeacons;
public:
    static bool             sRenderBeacons;
    static bool             sRenderHighlight;

    // Determines which set of UVs to use in highlight display
    //
    static LLRender::eTexIndex sRenderHighlightTextureChannel;

    // [F4] DoF focus target for the current frame, promoted from a local
    // static in renderDoF() so renderFocusPoint() can draw it in the UI pass.
    // (donor: Firestorm pipeline.h LLPipeline::sLastFocusPoint, FIRE-16728)
    static LLVector3        sLastFocusPoint;
    // [F4] whether DoF was actually composited this frame; gates the focus
    // point crosshair the same way donor's sDoFEnabled does (FIRE-32023).
    static bool             sDoFEnabled;

    //debug use
    static U32              sCurRenderPoolType ;

    //cached settings
    static bool WindLightUseAtmosShaders;
    static bool RenderDeferred;
    static F32 RenderDeferredSunWash;
    static U32 RenderFSAAType;
    static U32 RenderResolutionDivisor;
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
    static F32 RenderResolutionMultiplier;
// [/SL:KB]
    static bool RenderUIBuffer;
    static S32 RenderShadowDetail;
    static S32 RenderShadowSplits;
    static bool RenderDeferredSSAO;
    static F32 RenderShadowResolutionScale;
    static bool RenderDelayCreation;
    static bool RenderAnimateRes;
    static bool FreezeTime;
    static S32 DebugBeaconLineWidth;
    static F32 RenderHighlightBrightness;
    static LLColor4 RenderHighlightColor;
    static F32 RenderHighlightThickness;
    static bool RenderSpotLightsInNondeferred;
    static LLColor4 PreviewAmbientColor;
    static LLColor4 PreviewDiffuse0;
    static LLColor4 PreviewSpecular0;
    static LLColor4 PreviewDiffuse1;
    static LLColor4 PreviewSpecular1;
    static LLColor4 PreviewDiffuse2;
    static LLColor4 PreviewSpecular2;
    static LLVector3 PreviewDirection0;
    static LLVector3 PreviewDirection1;
    static LLVector3 PreviewDirection2;
    static F32 RenderGlowMinLuminance;
    static F32 RenderGlowMaxExtractAlpha;
    static F32 RenderGlowWarmthAmount;
    static LLVector3 RenderGlowLumWeights;
    static LLVector3 RenderGlowWarmthWeights;
    static S32 RenderGlowResolutionPow;
    static S32 RenderGlowIterations;
    static F32 RenderGlowWidth;
    static F32 RenderGlowStrength;
    static bool RenderGlowNoise;
    static bool RenderDepthOfField;
    static F32 CameraFocusTransitionTime;
    static F32 CameraFNumber;
    static F32 CameraFocalLength;
    static F32 CameraFieldOfView;
    static F32 RenderShadowNoise;
    static F32 RenderShadowBlurSize;
    static F32 RenderSSAOScale;
    static U32 RenderSSAOMaxScale;
    static F32 RenderSSAOFactor;
    static LLVector3 RenderSSAOEffect;
    static F32 RenderShadowOffsetError;
    static F32 RenderShadowBiasError;
    static F32 RenderShadowOffset;
    static F32 RenderShadowBias;
    static F32 RenderSpotShadowOffset;
    static F32 RenderSpotShadowBias;
    static LLDrawable* RenderSpotLight;
    static F32 RenderEdgeDepthCutoff;
    static F32 RenderEdgeNormCutoff;
    static LLVector3 RenderShadowGaussian;
    static F32 RenderShadowBlurDistFactor;
    static bool RenderDeferredAtmospheric;
    static F32 RenderHighlightFadeTime;
    static F32 RenderFarClip;
    static LLVector3 RenderShadowSplitExponent;
    static F32 RenderShadowErrorCutoff;
    static F32 RenderShadowFOVCutoff;
    static bool CameraOffset;
    static F32 CameraMaxCoF;
    static F32 CameraDoFResScale;
    static F32 RenderAutoHideSurfaceAreaLimit;
    static bool RenderScreenSpaceReflections;
    // [BDMerge G3.2] volumetric lighting (donor: Black Dragon)
    static bool RenderVolumetricLighting;
    static U32 RenderVolumetricLightingResolution;
    static F32 RenderVolumetricLightingMultiplier;
    static F32 RenderVolumetricLightingFalloffMultiplier;
    // [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts)
    static bool BDMergeProjectorVolumetrics;
    static U32 BDMergeProjectorVolumetricsResolution;
    static F32 BDMergeProjectorVolumetricsMultiplier;
    static F32 BDMergeProjectorVolumetricsAnisotropy;
    // [BDMerge G3.3 Phase 1] cinema levers
    static U32 BDMergeProjectorVolumetricsDither;
    static F32 BDMergeProjectorVolumetricsFeather;
    static U32 BDMergeProjectorVolumetricsShadowSamples;
    static bool BDMergeProjectorVolumetricsScissor;
    static bool BDMergeProjectorVolumetricsAdaptive;
    // [BDMerge G3.3 Phase 1 item 3] march the cones into a half-res target and
    // depth-aware bilateral-upsample the result onto the scene (big FPS win).
    static bool BDMergeProjectorVolumetricsHalfRes;
    static U32 BDMergeProjectorVolumetricsMinResolution;
    static F32 BDMergeProjectorVolumetricsMaxLuminance;
    // [BDMerge G3.3 Phase 2] global art-direction overrides for flagged shafts.
    static LLColor3 BDMergeProjectorVolumetricsTint;
    static F32 BDMergeProjectorVolumetricsTintStrength;
    // [BDMerge G3.3 Phase 3] atmosphere levers (all default to a no-op).
    static F32 BDMergeProjectorVolumetricsDensity;       // item 3: global haziness
    static F32 BDMergeProjectorVolumetricsNoiseStrength; // item 1: animated noise
    static F32 BDMergeProjectorVolumetricsNoiseScale;
    static F32 BDMergeProjectorVolumetricsNoiseSpeed;
    static F32 BDMergeProjectorVolumetricsFogStrength;   // item 2: height fog
    static F32 BDMergeProjectorVolumetricsFogGroundDensity;
    static F32 BDMergeProjectorVolumetricsFogFalloff;
    static F32 BDMergeProjectorVolumetricsFogBase;
    static F32 BDMergeProjectorVolumetricsBloomFeed;     // item 4: bloom halo feed
    // [BDMerge G3.3 Batch 1 A] across-frame temporal reprojection accumulation.
    static bool BDMergeProjectorVolumetricsTemporal;     // A: enable (default on)
    static F32 BDMergeProjectorVolumetricsTemporalBlend; // A: history EMA weight
    // [BDMerge G3.3 Batch 1 B] gobo-colored occluder shadows (stained-glass tint).
    static F32 BDMergeProjectorVolumetricsShadowTint;    // B: 0 = classic black shadow
    // [BDMerge G3.3 Rim] physical surface-coupled rim / wrap glow (auto-rim analog).
    static F32 BDMergeProjectorVolumetricsRimStrength;   // master brightness (0 = off)
    static F32 BDMergeProjectorVolumetricsRimPower;      // Fresnel exponent (silhouette tightness)
    static F32 BDMergeProjectorVolumetricsRimThreshold;  // ignore incident light dimmer than this
    static F32 BDMergeProjectorVolumetricsRimWrap;       // directional wrap (0 = back-only, 1 = broad)
    // [BDMerge G3.3 S-Log] physically-plausible beam levers (all default 0 = off).
    static F32 BDMergeProjectorVolumetricsExtinction;    // Beer-Lambert per-metre extinction
    static F32 BDMergeProjectorVolumetricsContactFade;   // soft surface-contact fade band (metres)
    static F32 BDMergeProjectorVolumetricsContactPool;   // surface-landing in-scatter deposit
    static F32 BDMergeProjectorVolumetricsSoftKnee;      // highlight soft-knee blend (0 = hard clamp)
    // [BDMerge Batch 2] Feature 1: soft (contact-hardening + filled) shadows.
    static bool BDMergeSoftProjectorShadows;   // master gate (default off)
    static F32  BDMergeSoftShadowSoftness;     // penumbra rate (kernel growth)
    static F32  BDMergeSoftShadowMaxPenumbra;  // max penumbra kernel radius (texels)
    static F32  BDMergeSoftShadowFill;         // ambient fill floor (0 = none)
    static bool BDMergeSoftShadowSun;          // also apply to the sun cascades
    // [BDMerge Batch 2] Feature 2: gobo/cookie mip + anisotropic filtering.
    static bool BDMergeGoboAnisotropic;        // default on
    // [BDMerge A5.4-1a] velocity / motion-vector buffer (Phase 1a foundation).
    static bool BDMergeVelocityBuffer;         // master gate, default OFF (extra geom pass)
    static bool BDMergeVelocityDebug;          // blit velocityMap to screen for validation
    static S32  BDMergeMotionBlurStrength;     // Phase 3 / debug-viz gain, default 32
    // [BDMerge G3.3 Phase 2] session-only opt-in set of projector object UUIDs
    // (not persisted; see toggleVolumetricShaft/clearVolumetricShafts).
    static std::set<LLUUID> sVolumetricShaftObjects;

    // [BDMerge G3.3 Batch 3] session-only opt-OUT set of projector object UUIDs
    // that should light but cast NO shadow (not persisted; see
    // toggleProjectorCastShadows/clearVolumetricShafts).
    static std::set<LLUUID> sNoShadowProjectors;

    // [BDMerge G3.3 Batch 1 C] Session-only PER-PROJECTOR art-direction overrides.
    // When a flagged projector has an override, the render loop uses these values
    // in place of the global sliders for that cone. Set via the right-click
    // "Shaft: capture current settings" action (snapshots the current globals) and
    // removed via "Shaft: clear override". NOT persisted - cleared on relog with
    // the flag set. A projector with no entry here falls back to the globals.
    struct VolumetricShaftOverride
    {
        F32      multiplier   = 1.f;
        F32      feather      = 0.15f;
        F32      anisotropy   = 0.72f;
        F32      density      = 1.f;
        LLColor3 tint         = LLColor3(1.f, 1.f, 1.f);
        F32      tintStrength = 0.f;
    };
    static void  setVolumetricShaftOverride(const LLUUID& id, const VolumetricShaftOverride& ov);
    static void  clearVolumetricShaftOverride(const LLUUID& id);
    static bool  getVolumetricShaftOverride(const LLUUID& id, VolumetricShaftOverride& out);
    static bool  hasVolumetricShaftOverride(const LLUUID& id);
    static std::map<LLUUID, VolumetricShaftOverride> sVolumetricShaftOverrides;
    static S32 RenderScreenSpaceReflectionIterations;
    static F32 RenderScreenSpaceReflectionRayStep;
    static F32 RenderScreenSpaceReflectionDistanceBias;
    static F32 RenderScreenSpaceReflectionDepthRejectBias;
    static F32 RenderScreenSpaceReflectionAdaptiveStepMultiplier;
    static S32 RenderScreenSpaceReflectionGlossySamples;
    static S32 RenderBufferVisualization;
    static bool RenderMirrors;
    static S32 RenderHeroProbeUpdateRate;
    static S32 RenderHeroProbeConservativeUpdateMultiplier;
    static bool RenderAvatarCloth;
};

void render_bbox(const LLVector3 &min, const LLVector3 &max);
void render_hud_elements();

extern LLPipeline gPipeline;
extern bool gDebugPipeline;
extern const LLMatrix4* gGLLastMatrix;

#endif
