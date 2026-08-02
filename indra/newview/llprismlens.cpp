/**
 * @file llprismlens.cpp
 * @brief Phase 0 projection/target spike for the Prism Lens feature.
 */

#include "llviewerprecompiledheaders.h"

#include "llprismlens.h"

#include "llappviewer.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "pipeline.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>

extern bool gCubeSnapshot;
extern bool gSnapshot;

namespace
{
constexpr S32 DEBUG_RECT_X = 96;
constexpr S32 DEBUG_RECT_Y = 96;
constexpr S32 DEBUG_RECT_WIDTH = 512;
constexpr S32 DEBUG_RECT_HEIGHT = 288;
constexpr F32 DEBUG_ZOOM = 2.f;

struct PrismDebugRect
{
    S32 mX = 0;
    S32 mY = 0;
    U32 mWidth = 0;
    U32 mHeight = 0;
};

PrismDebugRect sDebugRect;
S32 sMainViewport[4] = { 0, 0, 0, 0 };
U32 sRenderedFrame = 0;
bool sHasRenderedFrame = false;

bool prismEnabled()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "PrismLensEnabled", false);
    return enabled;
}

bool prismDebugEnabled()
{
    static LLCachedControl<bool> debug(gSavedSettings, "PrismLensDebug", false);
    return debug;
}

bool makeDebugRect(const S32 viewport[4], PrismDebugRect& rect)
{
    if (viewport[2] < 64 || viewport[3] < 64)
    {
        return false;
    }

    rect.mWidth = static_cast<U32>(llmin(viewport[2], DEBUG_RECT_WIDTH));
    rect.mHeight = static_cast<U32>(llmin(viewport[3], DEBUG_RECT_HEIGHT));
    rect.mX = viewport[0] + llmin(DEBUG_RECT_X, viewport[2] - static_cast<S32>(rect.mWidth));
    rect.mY = viewport[1] + llmin(DEBUG_RECT_Y, viewport[3] - static_cast<S32>(rect.mHeight));
    return true;
}

LLRender::eBlendFactor blendFactorFromGL(GLint factor)
{
    switch (factor)
    {
        case GL_ONE:                     return LLRender::BF_ONE;
        case GL_ZERO:                    return LLRender::BF_ZERO;
        case GL_DST_COLOR:               return LLRender::BF_DEST_COLOR;
        case GL_SRC_COLOR:               return LLRender::BF_SOURCE_COLOR;
        case GL_ONE_MINUS_DST_COLOR:     return LLRender::BF_ONE_MINUS_DEST_COLOR;
        case GL_ONE_MINUS_SRC_COLOR:     return LLRender::BF_ONE_MINUS_SOURCE_COLOR;
        case GL_DST_ALPHA:               return LLRender::BF_DEST_ALPHA;
        case GL_SRC_ALPHA:               return LLRender::BF_SOURCE_ALPHA;
        case GL_ONE_MINUS_DST_ALPHA:     return LLRender::BF_ONE_MINUS_DEST_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA:     return LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
        default:                         return LLRender::BF_UNDEF;
    }
}

struct ScopedPrismRenderState
{
    ScopedPrismRenderState()
        : mSavedCamera(LLViewerCamera::instance()),
          mSavedProjection(get_current_projection()),
          mSavedModelview(get_current_modelview()),
          mSavedLastProjection(get_last_projection()),
          mSavedLastModelview(get_last_modelview()),
          mSavedGLProjection(gGL.getMatrix(LLRender::MM_PROJECTION)),
          mSavedGLModelview(gGL.getMatrix(LLRender::MM_MODELVIEW)),
          mSavedDeltaModelview(gGLDeltaModelView),
          mSavedInverseDeltaModelview(gGLInverseDeltaModelView),
          mSavedMatrixMode(gGL.getMatrixMode()),
          mSavedRT(gPipeline.mRT),
          mSavedCameraID(LLViewerCamera::sCurCameraID),
          mSavedOcclusion(LLPipeline::sUseOcclusion),
          mSavedUnderWater(LLPipeline::sUnderWaterRender),
          mSavedPrismRender(LLPipeline::sPrismLensRender),
          mSavedVisibleLightCount(LLPipeline::sVisibleLightCount),
          mSavedVisibleFaces(gPipeline.mNumVisibleFaces),
          mSavedVisibleNodes(gPipeline.mNumVisibleNodes),
          mSavedBoundTarget(LLRenderTarget::getCurrentBoundTarget()),
          mSavedShader(LLGLSLShader::sCurBoundShaderPtr),
          mBlendState(GL_BLEND, LLGLState::DISABLED_STATE),
          mDepthState(GL_TRUE, GL_TRUE, GL_LEQUAL)
    {
        std::memcpy(mSavedGlobalViewport, gGLViewport, sizeof(mSavedGlobalViewport));
        glGetIntegerv(GL_VIEWPORT, mSavedGLViewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, mSavedColorMask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, mSavedClearColor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &mSavedBlend[0]);
        glGetIntegerv(GL_BLEND_DST_RGB, &mSavedBlend[1]);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSavedBlend[2]);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mSavedBlend[3]);

        gPipeline.pushRenderTypeMask();
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true, true);
    }

    ~ScopedPrismRenderState()
    {
        LLRenderTarget* bound_target = LLRenderTarget::getCurrentBoundTarget();
        if (bound_target != mSavedBoundTarget && bound_target)
        {
            bound_target->flush();
        }

        gPipeline.popRenderTypeMask();
        gPipeline.mRT = mSavedRT;
        LLPipeline::sPrismLensRender = mSavedPrismRender;
        LLPipeline::sUseOcclusion = mSavedOcclusion;
        LLPipeline::sUnderWaterRender = mSavedUnderWater;
        LLPipeline::sVisibleLightCount = mSavedVisibleLightCount;
        gPipeline.mNumVisibleFaces = mSavedVisibleFaces;
        gPipeline.mNumVisibleNodes = mSavedVisibleNodes;
        LLViewerCamera::sCurCameraID = mSavedCameraID;

        LLViewerCamera::instance() = mSavedCamera;
        set_current_projection(mSavedProjection);
        set_current_modelview(mSavedModelview);
        set_last_projection(mSavedLastProjection);
        set_last_modelview(mSavedLastModelview);
        gGLDeltaModelView = mSavedDeltaModelview;
        gGLInverseDeltaModelView = mSavedInverseDeltaModelview;

        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadMatrix(glm::value_ptr(mSavedGLProjection));
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(glm::value_ptr(mSavedGLModelview));
        gGL.matrixMode(mSavedMatrixMode);
        gGL.syncMatrices();

        std::memcpy(gGLViewport, mSavedGlobalViewport, sizeof(mSavedGlobalViewport));
        glViewport(mSavedGLViewport[0], mSavedGLViewport[1],
                   mSavedGLViewport[2], mSavedGLViewport[3]);
        glClearColor(mSavedClearColor[0], mSavedClearColor[1],
                     mSavedClearColor[2], mSavedClearColor[3]);

        gGL.setColorMask(mSavedColorMask[0] != GL_FALSE,
                         mSavedColorMask[1] != GL_FALSE,
                         mSavedColorMask[2] != GL_FALSE,
                         mSavedColorMask[3] != GL_FALSE);

        const LLRender::eBlendFactor src_rgb = blendFactorFromGL(mSavedBlend[0]);
        const LLRender::eBlendFactor dst_rgb = blendFactorFromGL(mSavedBlend[1]);
        const LLRender::eBlendFactor src_alpha = blendFactorFromGL(mSavedBlend[2]);
        const LLRender::eBlendFactor dst_alpha = blendFactorFromGL(mSavedBlend[3]);
        if (src_rgb != LLRender::BF_UNDEF && dst_rgb != LLRender::BF_UNDEF &&
            src_alpha != LLRender::BF_UNDEF && dst_alpha != LLRender::BF_UNDEF)
        {
            gGL.blendFunc(src_rgb, dst_rgb, src_alpha, dst_alpha);
        }
        else
        {
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
        }

        if (mSavedShader && mSavedShader->isComplete())
        {
            mSavedShader->bind();
        }
        else
        {
            LLGLSLShader::unbind();
        }

        llassert(LLRenderTarget::getCurrentBoundTarget() == mSavedBoundTarget);
    }

    LLViewerCamera mSavedCamera;
    glm::mat4 mSavedProjection;
    glm::mat4 mSavedModelview;
    glm::mat4 mSavedLastProjection;
    glm::mat4 mSavedLastModelview;
    glm::mat4 mSavedGLProjection;
    glm::mat4 mSavedGLModelview;
    glm::mat4 mSavedDeltaModelview;
    glm::mat4 mSavedInverseDeltaModelview;
    LLRender::eMatrixMode mSavedMatrixMode;
    S32 mSavedGlobalViewport[4];
    GLint mSavedGLViewport[4];
    GLboolean mSavedColorMask[4];
    GLfloat mSavedClearColor[4];
    GLint mSavedBlend[4];
    LLPipeline::RenderTargetPack* mSavedRT;
    LLViewerCamera::eCameraID mSavedCameraID;
    S32 mSavedOcclusion;
    bool mSavedUnderWater;
    bool mSavedPrismRender;
    S32 mSavedVisibleLightCount;
    S32 mSavedVisibleFaces;
    S32 mSavedVisibleNodes;
    LLRenderTarget* mSavedBoundTarget;
    LLGLSLShader* mSavedShader;
    LLGLState mBlendState;
    LLGLDepthTest mDepthState;
};
} // anonymous namespace

namespace LLPrismLens
{
void renderAuxiliaryView()
{
    // This is the complete feature-off path: no allocation and no render state
    // is touched before this return.
    if (!prismEnabled())
    {
        return;
    }

    if (LLPipeline::sPrismLensRender || !LLPipeline::sRenderDeferred ||
        gCubeSnapshot || gSnapshot)
    {
        return;
    }

    sHasRenderedFrame = false;

    S32 main_viewport[4];
    std::memcpy(main_viewport, gGLViewport, sizeof(main_viewport));
    PrismDebugRect debug_rect;
    if (!makeDebugRect(main_viewport, debug_rect))
    {
        return;
    }

    ScopedPrismRenderState scoped_state;

    if (!gPipeline.allocatePrismLensBuffer(debug_rect.mWidth, debug_rect.mHeight))
    {
        return;
    }

    LLPipeline::sPrismLensRender = true;
    LLPipeline::sUseOcclusion = 0;
    LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_PRISM_LENS;
    gPipeline.mRT = &gPipeline.mPrismLensRT;

    LLViewerCamera& camera = LLViewerCamera::instance();
    const glm::vec2 crop_center(
        static_cast<F32>(debug_rect.mX) + static_cast<F32>(debug_rect.mWidth) * 0.5f,
        static_cast<F32>(debug_rect.mY) + static_cast<F32>(debug_rect.mHeight) * 0.5f);
    const glm::vec2 crop_size(
        static_cast<F32>(debug_rect.mWidth) / DEBUG_ZOOM,
        static_cast<F32>(debug_rect.mHeight) / DEBUG_ZOOM);
    const glm::ivec4 viewport(main_viewport[0], main_viewport[1],
                              main_viewport[2], main_viewport[3]);
    const glm::mat4 prism_projection =
        glm::pickMatrix(crop_center, crop_size, viewport) * get_current_projection();

    set_current_projection(prism_projection);
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.loadMatrix(glm::value_ptr(prism_projection));
    gGL.matrixMode(LLRender::MM_MODELVIEW);

    gGLViewport[0] = 0;
    gGLViewport[1] = 0;
    gGLViewport[2] = static_cast<S32>(debug_rect.mWidth);
    gGLViewport[3] = static_cast<S32>(debug_rect.mHeight);
    glViewport(0, 0, static_cast<GLsizei>(debug_rect.mWidth),
               static_cast<GLsizei>(debug_rect.mHeight));
    LLViewerCamera::updateFrustumPlanes(camera, false, false, true);

    static LLCullResult prism_cull;
    prism_cull.clear();
    gPipeline.updateCull(camera, prism_cull);
    gPipeline.stateSort(camera, prism_cull);

    LLPipeline::RenderTargetPack& rt = gPipeline.mPrismLensRT;
    rt.deferredScreen.bindTarget();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    rt.deferredScreen.clear();
    gPipeline.renderGeomDeferred(camera, false);
    rt.deferredScreen.flush();
    gPipeline.renderDeferredLighting();

    sDebugRect = debug_rect;
    std::memcpy(sMainViewport, main_viewport, sizeof(sMainViewport));
    sRenderedFrame = gFrameCount;
    sHasRenderedFrame = true;
}

void compositeDebug()
{
    if (!prismEnabled() || !prismDebugEnabled() ||
        LLPipeline::sPrismLensRender || !sHasRenderedFrame ||
        sRenderedFrame != gFrameCount ||
        !gPipeline.mPrismLensRT.screen.isComplete())
    {
        return;
    }

    LLRenderTarget& source = gPipeline.mPrismLensRT.screen;
    LLRenderTarget& destination = gPipeline.mMainRT.screen;
    const F32 scale_x = static_cast<F32>(destination.getWidth()) /
                        static_cast<F32>(sMainViewport[2]);
    const F32 scale_y = static_cast<F32>(destination.getHeight()) /
                        static_cast<F32>(sMainViewport[3]);
    const S32 destination_x = ll_round(
        static_cast<F32>(sDebugRect.mX - sMainViewport[0]) * scale_x);
    const S32 destination_y = ll_round(
        static_cast<F32>(sDebugRect.mY - sMainViewport[1]) * scale_y);
    const S32 destination_width = ll_round(
        static_cast<F32>(sDebugRect.mWidth) * scale_x);
    const S32 destination_height = ll_round(
        static_cast<F32>(sDebugRect.mHeight) * scale_y);
    destination.copyContents(
        source,
        0, 0, source.getWidth(), source.getHeight(),
        destination_x, destination_y,
        destination_x + destination_width,
        destination_y + destination_height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
}
} // namespace LLPrismLens
