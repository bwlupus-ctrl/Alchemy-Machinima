/**
 * @file llprismpreview.cpp
 * @brief Demand and scoped render policy for Director VCam previews.
 */

#include "llviewerprecompiledheaders.h"

#include "llprismpreview.h"

#include "llviewershadermgr.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "pipeline.h"
#include "llenvironment.h"
#include "llsettingssky.h"

namespace
{
constexpr LLPrismPreviewService::VisibilityOwner PAGE_VISIBILITY_OWNER = 1;
}

LLPrismPreviewService& LLPrismPreviewService::instance()
{
    static LLPrismPreviewService service;
    return service;
}

void LLPrismPreviewService::setDemand(const LLPrismLens::CaptureHandle& capture,
                                      bool demanded)
{
    if (capture.mId.isNull()) return;
    const U32 slot = LLPrismLens::captureSlot(capture);
    if (slot >= mDemand.size())
    {
        if (!demanded)
        {
            for (DemandEntry& stale : mDemand)
                if (stale.mCapture.mId == capture.mId) stale = DemandEntry{};
        }
        return;
    }
    DemandEntry& entry = mDemand[slot];
    const bool same_publication = entry.mCapture.mId == capture.mId &&
        entry.mCapture.mGeneration == capture.mGeneration;
    if (!same_publication) mNextAtlasDue[slot] = 0.0;
    entry = demanded
        ? DemandEntry{ capture, true,
                       same_publication ? entry.mPublicationSerial : 0,
                       same_publication ? entry.mAtlasSerial : 0,
                       same_publication ? entry.mPreviewQuality : false }
        : DemandEntry{};
}

void LLPrismPreviewService::clearDemand()
{
    for (DemandEntry& entry : mDemand) entry = DemandEntry{};
    for (LLPrismLens::DisplayHandle& selection : mSelection)
        selection = LLPrismLens::DisplayHandle{};
    for (TilePublication& tile : mTiles) tile = TilePublication{};
    mNextAtlasDue.fill(0.0);
    ++mRevision;
}

void LLPrismPreviewService::setPageVisible(bool visible)
{
    setConsumerVisible(PAGE_VISIBILITY_OWNER, visible);
}

void LLPrismPreviewService::setConsumerVisible(VisibilityOwner owner, bool visible)
{
    if (owner == 0) return;
    VisibilityEntry* empty = nullptr;
    for (VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner == owner)
        {
            entry.mVisible = visible;
            return;
        }
        if (!empty && entry.mOwner == 0) empty = &entry;
    }
    if (empty)
    {
        *empty = VisibilityEntry{ owner, visible };
        return;
    }
    llassert_always(false && "LLPrismPreviewService visibility owner table full");
}

void LLPrismPreviewService::releaseConsumer(VisibilityOwner owner)
{
    for (VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner == owner)
        {
            entry = VisibilityEntry{};
            return;
        }
    }
}

void LLPrismPreviewService::setMonitorSelection(
    U32 capture_slot, const LLPrismLens::DisplayHandle& display)
{
    if (capture_slot >= mSelection.size()) return;
    mSelection[capture_slot] = display;
    DemandEntry& entry = mDemand[capture_slot];
    if (entry.mPublicationSerial > 0)
        entry.mAtlasSerial = entry.mPublicationSerial - 1;
    mNextAtlasDue[capture_slot] = 0.0;
    ++mRevision;
}

bool LLPrismPreviewService::bindAtlas(const AtlasHandle& handle,
                                      S32 texture_channel)
{
    if (handle.mGeneration == 0 || handle.mGeneration != mAtlasGeneration ||
        !mAtlas.isComplete() || texture_channel < 0)
    {
        return false;
    }
    mAtlas.bindTexture(0, texture_channel, LLTexUnit::TFO_BILINEAR);
    return true;
}

LLPrismPreviewService::TilePublication LLPrismPreviewService::tileState(
    const LLPrismLens::CaptureHandle& capture) const
{
    for (U32 slot = 0; slot < mDemand.size(); ++slot)
    {
        const DemandEntry& entry = mDemand[slot];
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration)
        {
            TilePublication tile = mTiles[slot];
            static LLCachedControl<bool> previews_enabled(
                gSavedSettings, "PrismMonitorPreviewsEnabled", true);
            if (!previews_enabled) tile.mState = ETileState::PAUSED;
            return tile;
        }
    }
    return TilePublication{};
}

bool LLPrismPreviewService::anyVisibleConsumer() const
{
    for (const VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner != 0 && entry.mVisible) return true;
    }
    return false;
}

bool LLPrismPreviewService::anyDemand() const
{
    for (const DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded) return true;
    }
    return false;
}

bool LLPrismPreviewService::prepareFrame()
{
    static LLCachedControl<bool> previews_enabled(
        gSavedSettings, "PrismMonitorPreviewsEnabled", true);
    const bool enabled = previews_enabled;
    if (enabled != mLastPreviewsEnabled)
    {
        mLastPreviewsEnabled = enabled;
        const F64 now = LLTimer::getTotalSeconds();
        static LLCachedControl<F32> preview_fps(
            gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
        const F64 next = now + 1.0 / llclamp(static_cast<F64>(preview_fps()), 0.5, 5.0);
        mNextAtlasDue.fill(next);
        if (!enabled)
        {
            for (DemandEntry& entry : mDemand)
                entry.mAtlasSerial = entry.mPublicationSerial;
        }
        ++mRevision;
    }
    if (!enabled || !anyVisibleConsumer() || !anyDemand())
    {
        return false;
    }
    // Deliberately no GL work here: this runs before the complete auxiliary
    // render-state scope. Cold allocation is performed by ensureScratchReady().
    return true;
}

bool LLPrismPreviewService::ensureScratchReady()
{
    const F64 now = LLTimer::getTotalSeconds();
    if (now < mScratchRetryAfter) return false;
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("VCam preview scratch warmup");
    if (gPipeline.bindPrismPreviewScratch()) return true;
    if (!gPipeline.allocatePrismPreviewScratch())
    {
        mScratchRetryAfter = now + 1.0;
        return false;
    }
    mScratchRetryAfter = 0.0;
    return gPipeline.bindPrismPreviewScratch();
}

bool LLPrismPreviewService::isDemanded(
    const LLPrismLens::CaptureHandle& capture) const
{
    for (const DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration) return true;
    }
    return false;
}

F32 LLPrismPreviewService::requestedHz(
    const LLPrismLens::CaptureRateSettings& rate) const
{
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    return LLPrismPreviewPolicy::requestedHz(preview_fps, rate.mMode,
                                             rate.mTargetFps);
}

void LLPrismPreviewService::notePublication(
    const LLPrismLens::CaptureHandle& capture, U64 serial,
    bool preview_quality)
{
    for (DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration)
        {
            entry.mPublicationSerial = serial;
            entry.mPreviewQuality = preview_quality;
            return;
        }
    }
}

bool LLPrismPreviewService::hasPendingAtlasUpdate() const
{
    static LLCachedControl<bool> previews_enabled(
        gSavedSettings, "PrismMonitorPreviewsEnabled", true);
    if (!previews_enabled || !anyVisibleConsumer()) return false;
    const F64 now = LLTimer::getTotalSeconds();
    for (U32 slot = 0; slot < mDemand.size(); ++slot)
    {
        const DemandEntry& entry = mDemand[slot];
        if (entry.mDemanded && entry.mPublicationSerial > entry.mAtlasSerial)
            if (now >= mNextAtlasDue[slot]) return true;
    }
    return false;
}

namespace
{
constexpr U32 ATLAS_COLUMNS = 4;
constexpr U32 ATLAS_ROWS = 2;
constexpr U32 CELL_WIDTH = 256;
constexpr U32 CELL_HEIGHT = 144;
constexpr U32 ATLAS_WIDTH = ATLAS_COLUMNS * CELL_WIDTH;
constexpr U32 ATLAS_HEIGHT = ATLAS_ROWS * CELL_HEIGHT;

struct ScopedPreviewAtlasGL
{
    ScopedPreviewAtlasGL()
        : mShader(LLGLSLShader::sCurBoundShaderPtr),
          mCamera(LLViewerCamera::sCurCameraID), mPipelineRT(gPipeline.mRT),
          mScissor(glIsEnabled(GL_SCISSOR_TEST)),
          mBlend(glIsEnabled(GL_BLEND)), mDepth(glIsEnabled(GL_DEPTH_TEST)),
          mFramebufferSrgb(glIsEnabled(GL_FRAMEBUFFER_SRGB))
    {
        std::memcpy(mViewport, gGLViewport, sizeof(mViewport));
        glGetIntegerv(GL_SCISSOR_BOX, mScissorBox);
        glGetIntegerv(GL_BLEND_SRC_RGB, &mBlendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &mBlendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mBlendDstAlpha);
        glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, mColorMask);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &mActiveTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &mTexture0);
    }
    ~ScopedPreviewAtlasGL()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture0);
        glActiveTexture(mActiveTexture);
        if (mShader && mShader->isComplete()) mShader->bind();
        else LLGLSLShader::unbind();
        setEnabled(GL_SCISSOR_TEST, mScissor);
        glScissor(mScissorBox[0], mScissorBox[1], mScissorBox[2], mScissorBox[3]);
        setEnabled(GL_BLEND, mBlend);
        glBlendFuncSeparate(mBlendSrcRgb, mBlendDstRgb,
                            mBlendSrcAlpha, mBlendDstAlpha);
        setEnabled(GL_DEPTH_TEST, mDepth);
        glDepthFunc(mDepthFunc);
        glDepthMask(mDepthMask);
        gGL.setColorMask(mColorMask[0] != GL_FALSE, mColorMask[1] != GL_FALSE,
                         mColorMask[2] != GL_FALSE, mColorMask[3] != GL_FALSE);
        setEnabled(GL_FRAMEBUFFER_SRGB, mFramebufferSrgb);
        std::memcpy(gGLViewport, mViewport, sizeof(mViewport));
        glViewport(mViewport[0], mViewport[1], mViewport[2], mViewport[3]);
        LLViewerCamera::sCurCameraID = mCamera;
        gPipeline.mRT = mPipelineRT;
    }
    static void setEnabled(GLenum cap, GLboolean enabled)
    {
        if (enabled) glEnable(cap); else glDisable(cap);
    }
    LLGLSLShader* mShader;
    LLViewerCamera::eCameraID mCamera;
    LLPipeline::RenderTargetPack* mPipelineRT;
    S32 mViewport[4];
    GLboolean mScissor, mBlend, mDepth, mFramebufferSrgb;
    GLint mScissorBox[4], mBlendSrcRgb, mBlendDstRgb;
    GLint mBlendSrcAlpha, mBlendDstAlpha, mDepthFunc;
    GLboolean mDepthMask, mColorMask[4];
    GLint mActiveTexture, mTexture0;
};

void uploadSignalUniforms(LLGLSLShader& shader,
                          const LLPrismLens::CompositeState& state,
                          F32 effect_time)
{
    static const LLStaticHashedString sScale("displayToCaptureScale");
    static const LLStaticHashedString sOffset("displayToCaptureOffset");
    static const LLStaticHashedString sOrientationScale("retainedOrientationScale");
    static const LLStaticHashedString sOrientationOffset("retainedOrientationOffset");
    static const LLStaticHashedString sRegionScale("textureRegionScale");
    static const LLStaticHashedString sRegionOffset("textureRegionOffset");
    static const LLStaticHashedString sLetterbox("letterbox");
    static const LLStaticHashedString sBars("barColorLinear");
    static const LLStaticHashedString sFeather("edgeFeather");
    static const LLStaticHashedString sOptics("prismLensOptics");
    static const LLStaticHashedString sEffect0("screenEffect0");
    static const LLStaticHashedString sEffect1("screenEffect1");
    static const LLStaticHashedString sEffect2("screenEffect2");
    static const LLStaticHashedString sEffect3("screenEffect3");
    static const LLStaticHashedString sEffect4("screenEffect4");
    static const LLStaticHashedString sEffect5("screenEffect5");
    static const LLStaticHashedString sTime("screenEffectTime");
    shader.uniform2fv(sScale, 1, state.mDisplayToCaptureScale);
    shader.uniform2fv(sOffset, 1, state.mDisplayToCaptureOffset);
    shader.uniform2fv(sOrientationScale, 1, state.mRetainedOrientationScale);
    shader.uniform2fv(sOrientationOffset, 1, state.mRetainedOrientationOffset);
    shader.uniform2fv(sRegionScale, 1, state.mTextureRegionScale);
    shader.uniform2fv(sRegionOffset, 1, state.mTextureRegionOffset);
    shader.uniform1i(sLetterbox, state.mLetterbox);
    shader.uniform3fv(sBars, 1, state.mBarColorLinear);
    shader.uniform1f(sFeather, state.mEdgeFeather);
    shader.uniform4fv(sOptics, 1, state.mOpticsParams);
    shader.uniform4fv(sEffect0, 1, state.mScreenEffect0);
    shader.uniform4fv(sEffect1, 1, state.mScreenEffect1);
    shader.uniform4fv(sEffect2, 1, state.mScreenEffect2);
    shader.uniform4fv(sEffect3, 1, state.mScreenEffect3);
    shader.uniform4fv(sEffect4, 1, state.mScreenEffect4);
    shader.uniform4fv(sEffect5, 1, state.mScreenEffect5);
    shader.uniform1f(sTime, effect_time);
}
} // namespace

void LLPrismPreviewService::updateAtlas()
{
    if (!hasPendingAtlasUpdate()) return; // Feature-off: no GL query or mutation.
    if (mLastAtlasFrame == gFrameCount) return;
    mLastAtlasFrame = gFrameCount;
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism preview atlas composite");

    U32 cell = static_cast<U32>(mDemand.size());
    const F64 now = LLTimer::getTotalSeconds();
    for (U32 offset = 0; offset < mDemand.size(); ++offset)
    {
        const U32 candidate = (mNextAtlasSlot + offset) % mDemand.size();
        const DemandEntry& entry = mDemand[candidate];
        if (entry.mDemanded && entry.mPublicationSerial > entry.mAtlasSerial &&
            now >= mNextAtlasDue[candidate])
        {
            cell = candidate;
            break;
        }
    }
    if (cell >= mDemand.size()) return;

    DemandEntry& entry = mDemand[cell];
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    mNextAtlasDue[cell] = now +
        1.0 / llclamp(static_cast<F64>(preview_fps()), 0.5, 5.0);
    LLPrismLens::CompositeState state;
    bool offline = false;
    if (!LLPrismLens::buildPreviewCompositeState(
            entry.mCapture, mSelection[cell],
            static_cast<F32>(CELL_WIDTH) / static_cast<F32>(CELL_HEIGHT),
            state, offline))
    {
        TilePublication& tile = mTiles[cell];
        tile.mState = ETileState::OFFLINE;
        tile.mTimestamp = LLTimer::getTotalSeconds();
        tile.mPublicationSerial = entry.mPublicationSerial;
        entry.mAtlasSerial = entry.mPublicationSerial;
        ++mRevision;
        mNextAtlasSlot = (cell + 1) % mDemand.size();
        return;
    }

    const U32 permutation = gPipeline.colorCorrectPermutation(true, true);
    LLGLSLShader& shader = gPrismPreviewProgram[permutation];
    if (!shader.isComplete())
    {
        mTiles[cell].mState = ETileState::OFFLINE;
        mTiles[cell].mTimestamp = LLTimer::getTotalSeconds();
        mTiles[cell].mPublicationSerial = entry.mPublicationSerial;
        entry.mAtlasSerial = entry.mPublicationSerial;
        mNextAtlasSlot = (cell + 1) % mDemand.size();
        ++mRevision;
        return;
    }

    ScopedPreviewAtlasGL restore;
    if (!mAtlas.isComplete())
    {
        // Shader writes explicit display-sRGB code values, so use non-decoding
        // RGBA8 storage; LLUI consumes those values as display pixels.
        if (!mAtlas.allocate(ATLAS_WIDTH, ATLAS_HEIGHT, GL_RGBA8)) return;
        ++mAtlasGeneration;
    }

    const U32 column = cell % ATLAS_COLUMNS;
    const U32 row = cell / ATLAS_COLUMNS;
    const S32 x = static_cast<S32>(column * CELL_WIDTH);
    const S32 y = static_cast<S32>((ATLAS_ROWS - 1u - row) * CELL_HEIGHT);
    mAtlas.bindTarget();
    glViewport(x, y, CELL_WIDTH, CELL_HEIGHT);
    const S32 atlas_viewport[4] = {
        x, y, static_cast<S32>(CELL_WIDTH), static_cast<S32>(CELL_HEIGHT) };
    std::memcpy(gGLViewport, atlas_viewport, sizeof(gGLViewport));
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, CELL_WIDTH, CELL_HEIGHT);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    gGL.setColorMask(true, true, true, true);
    glDisable(GL_FRAMEBUFFER_SRGB);

    shader.bind();
    const S32 signal_channel = shader.enableTexture(LLShaderMgr::PRISM_LENS_MAP);
    if (signal_channel >= 0)
    {
        gPipeline.mPrismLensOutput[state.mCaptureSlot].bindTexture(
            0, signal_channel, LLTexUnit::TFO_BILINEAR);
    }
    const S32 exposure_channel = shader.enableTexture(LLShaderMgr::EXPOSURE_MAP);
    if (exposure_channel >= 0)
    {
        gPipeline.mExposureMap.bindTexture(0, exposure_channel, LLTexUnit::TFO_POINT);
    }
    uploadSignalUniforms(shader, state, fmodf(gFrameTimeSeconds, 3600.f));
    static const LLStaticHashedString sSheenSky("prismEnvSheenSky");
    static const LLStaticHashedString sSheenGround("prismEnvSheenGround");
    LLColor3 sheen_sky(0.f, 0.f, 0.f);
    LLColor3 sheen_ground(0.f, 0.f, 0.f);
    if (const LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky())
    {
        const LLColor3 ambient = sky->getAmbientColor();
        sheen_sky = ambient * 1.5f + sky->getBlueHorizon() * 0.4f;
        sheen_ground = ambient * 0.5f;
    }
    shader.uniform3fv(sSheenSky, 1, sheen_sky.mV);
    shader.uniform3fv(sSheenGround, 1, sheen_ground.mV);
    static const LLStaticHashedString sWorldOrigin("previewWorldOrigin");
    static const LLStaticHashedString sWorldU("previewWorldUEdge");
    static const LLStaticHashedString sWorldV("previewWorldVEdge");
    const LLVector3 world_origin(state.mVirtualCorners[0]);
    const LLVector3 world_u = LLVector3(state.mVirtualCorners[1]) - world_origin;
    const LLVector3 world_v = LLVector3(state.mVirtualCorners[3]) - world_origin;
    shader.uniform3fv(sWorldOrigin, 1, world_origin.mV);
    shader.uniform3fv(sWorldU, 1, world_u.mV);
    shader.uniform3fv(sWorldV, 1, world_v.mV);
    gPipeline.uploadPrismPreviewColorCorrectUniforms(shader);
    const S32 color_grade_channel =
        gPipeline.uploadPrismPreviewColorGradeUniforms(shader);

    LLVertexBuffer::unbind();
    gGL.begin(LLRender::TRIANGLES);
    gGL.vertex3f(-1.f, -1.f, 0.f);
    gGL.vertex3f(3.f, -1.f, 0.f);
    gGL.vertex3f(-1.f, 3.f, 0.f);
    gGL.end();
    shader.disableTexture(LLShaderMgr::EXPOSURE_MAP);
    shader.disableTexture(LLShaderMgr::PRISM_LENS_MAP);
    if (color_grade_channel >= 0)
        gGL.getTexUnit(color_grade_channel)->unbind(LLTexUnit::TT_TEXTURE_3D);
    mAtlas.flush();

    entry.mAtlasSerial = entry.mPublicationSerial;
    TilePublication& tile = mTiles[cell];
    tile.mAtlas = atlasHandle();
    const F32 left = static_cast<F32>(column) / ATLAS_COLUMNS;
    const F32 right = static_cast<F32>(column + 1u) / ATLAS_COLUMNS;
    const F32 top = 1.f - static_cast<F32>(row) / ATLAS_ROWS;
    const F32 bottom = 1.f - static_cast<F32>(row + 1u) / ATLAS_ROWS;
    tile.mCellUV = LLRectf(left, top, right, bottom);
    LLPrismLens::EActivityState activity = LLPrismLens::EActivityState::IDLE;
    bool healthy = false;
    LLPrismLens::previewTileActivity(entry.mCapture, activity, healthy);
    if (!healthy) tile.mState = ETileState::OFFLINE;
    else if (activity == LLPrismLens::EActivityState::PAUSED)
        tile.mState = ETileState::PAUSED;
    else if (activity == LLPrismLens::EActivityState::THROTTLED)
        tile.mState = ETileState::THROTTLED;
    else tile.mState = entry.mPreviewQuality ? ETileState::PREVIEW : ETileState::LIVE;
    tile.mTimestamp = LLTimer::getTotalSeconds();
    tile.mPublicationSerial = entry.mPublicationSerial;
    ++mRevision;
    mNextAtlasSlot = (cell + 1) % mDemand.size();
}

void LLPrismPreviewService::onRenderTargetsReleased()
{
    // No GL name or LLRenderTarget pointer escapes the pipeline. Publication
    // serials are invalid until their captures publish again in the new context.
    for (DemandEntry& entry : mDemand) entry.mPublicationSerial = 0;
    mAtlas.release();
    ++mAtlasGeneration;
    ++mRevision;
    mScratchRetryAfter = 0.0;
}

ScopedPrismPreviewQuality::ScopedPrismPreviewQuality()
    : mShadowDetail(LLPipeline::RenderShadowDetail),
      mDeferredSSAO(LLPipeline::RenderDeferredSSAO),
      mVolumetricLighting(LLPipeline::RenderVolumetricLighting),
      mProjectorVolumetrics(LLPipeline::BDMergeProjectorVolumetrics),
      mFarClip(LLPipeline::RenderFarClip),
      mPreviewRender(LLPipeline::sPrismPreviewRender)
{
    LLPipeline::sPrismPreviewRender = true;
    LLPipeline::RenderShadowDetail = 0;
    LLPipeline::RenderDeferredSSAO = false;
    LLPipeline::RenderVolumetricLighting = false;
    LLPipeline::BDMergeProjectorVolumetrics = false;
    LLPipeline::RenderFarClip = llmin(mFarClip, LLPrismPreviewPolicy::MAX_FAR_CLIP);
}

ScopedPrismPreviewQuality::~ScopedPrismPreviewQuality()
{
    LLPipeline::RenderFarClip = mFarClip;
    LLPipeline::BDMergeProjectorVolumetrics = mProjectorVolumetrics;
    LLPipeline::RenderVolumetricLighting = mVolumetricLighting;
    LLPipeline::RenderDeferredSSAO = mDeferredSSAO;
    LLPipeline::RenderShadowDetail = mShadowDetail;
    LLPipeline::sPrismPreviewRender = mPreviewRender;
}
