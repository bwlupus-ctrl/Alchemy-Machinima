#include "llviewerprecompiledheaders.h"
#include "alcolorchart.h"
#include "alcolorchartdata.h"
#include "llagent.h"
#include "llapp.h"
#include "llbutton.h"
#include "lldrawable.h"
#include "llimage.h"
#include "llframetimer.h"
#include "llnotificationsutil.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llvolumemgr.h"
#include "pipeline.h"
#include <cmath>
#include <glm/gtc/type_ptr.hpp>

namespace
{
const LLStaticHashedString MATRIX("chart_matrix");
const LLStaticHashedString COLOUR("chart_colour");
const LLStaticHashedString SAMPLE("chart_sample");
const LLStaticHashedString EXPOSED("chart_exposed");
const LLStaticHashedString VIEWPORT("chart_viewport");
ALColorChart* sChart = nullptr;

bool unlit()
{
    static LLCachedControl<S32> mode(gSavedSettings, "AlchemyColorChartMode", 0);
    return mode() == 0;
}

// The caller first verifies that final presentation is on the window.
// Read binding and pack state are independent of the draw framebuffer.
class ScopedReadbackState
{
public:
    ScopedReadbackState()
    {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFramebuffer);
        glGetIntegerv(GL_READ_BUFFER, &mReadBuffer);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &mPackBuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &mPackAlignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &mPackLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &mPackRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &mPackPixels);
        glGetIntegerv(GL_PACK_SWAP_BYTES, &mPackSwap);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    }

    ~ScopedReadbackState()
    {
        glPixelStorei(GL_PACK_ALIGNMENT, mPackAlignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, mPackLength);
        glPixelStorei(GL_PACK_SKIP_ROWS, mPackRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, mPackPixels);
        glPixelStorei(GL_PACK_SWAP_BYTES, mPackSwap);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(mPackBuffer));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(mReadFramebuffer));
        glReadBuffer(static_cast<GLenum>(mReadBuffer));
    }

private:
    GLint mReadFramebuffer = 0;
    GLint mReadBuffer = 0;
    GLint mPackBuffer = 0;
    GLint mPackAlignment = 0;
    GLint mPackLength = 0;
    GLint mPackRows = 0;
    GLint mPackPixels = 0;
    GLint mPackSwap = 0;
};
}

ALColorChart& ALColorChart::instance()
{
    static ALColorChart chart;
    return chart;
}

ALColorChart::ALColorChart()
{
    sChart = this;
    mTeleportConnection = LLViewerParcelMgr::getInstance()->setTeleportFinishedCallback(
        [this](const LLVector3d&, const bool&)
        {
            mCheckRegionPin = true;
        });
    // Hide releases all chart resources; hidden idle never enters the singleton.
    mShowConnection = gSavedSettings.getControl("AlchemyColorChartShow")->getSignal()->connect(
        [this](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            if (!value.asBoolean())
            {
                releaseResources();
            }
        });
    mPlacementConnection = gSavedSettings.getControl("AlchemyColorChartPlacement")->getSignal()->connect(
        [this](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            if (value.asInteger() == 0)
            {
                mPinned = false;
            }
            else if (!mPinned)
            {
                pinHere();
            }
        });
}

ALColorChart::~ALColorChart()
{
    // shutdownSession releases GPU resources while the shader manager and
    // context are alive. Do not recreate either during static destruction.
    destroyPrim();
    sChart = nullptr;
}

void ALColorChart::shutdownSession()
{
    gSavedSettings.setBOOL("AlchemyColorChartShow", false);
    gSavedSettings.setS32("AlchemyColorChartPlacement", 0);
    if (sChart)
    {
        sChart->releaseResources();
        sChart->mPinned = false;
        sChart->mCheckRegionPin = false;
        sChart->mPosition = LLVector3d::zero;
        sChart->mRotation = LLQuaternion();
        sChart->mScale = LLVector3::zero;
    }
}

void ALColorChart::toggle()
{
    gSavedSettings.setBOOL("AlchemyColorChartShow", !gSavedSettings.getBOOL("AlchemyColorChartShow"));
}

void ALColorChart::bind(LLView* root)
{
    if (LLButton* pin = root->findChild<LLButton>("color_chart_pin"))
    {
        pin->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            instance().pinHere();
        });
    }
}

void ALColorChart::destroyPrim()
{
    LLPointer<LLVOVolume> dying = mPrim;
    mPrim = nullptr;
    if (dying.notNull() && !dying->isDead())
    {
        dying->markDead();
    }
}

void ALColorChart::releaseResources()
{
    destroyPrim();
    mLinear.release();
    mExposed.release();
    mTexture = nullptr;
    if (mShaderResourcesRequested)
    {
        LLViewerShaderMgr::instance()->unloadColorChartShaders();
        mShaderResourcesRequested = false;
    }
    mAtlasFailed = false;
    mLastDrawFrame = ~0u;
    mReadbackFrame = 0;
}

bool ALColorChart::ensureShaders(bool legacy)
{
    auto* manager = LLViewerShaderMgr::instance();
    const U64 generation = manager->colorChartShaderGeneration();
    if (mShaderGeneration != generation)
    {
        mLinear.release();
        mExposed.release();
        mAtlasFailed = false;
        mShaderGeneration = generation;
    }
    mShaderResourcesRequested = true;
    return manager->loadColorChartShaders(legacy);
}

void ALColorChart::cameraTransform(LLVector3d& position, LLQuaternion& rotation, LLVector3& scale) const
{
    const auto* camera = LLViewerCamera::getInstance();
    const F32 distance = llmax(2.f, camera->getNear() + .1f);
    const F32 tangent = tanf(camera->getView() * .5f);
    static LLCachedControl<F32> size(gSavedSettings, "AlchemyColorChartSize", .25f);
    static LLCachedControl<S32> anchor_setting(gSavedSettings, "AlchemyColorChartAnchor", 0);
    const F32 fraction = llclamp(size(), .10f, .60f);
    const F32 anchor = anchor_setting() == 1 ? -.45f : 0.f;
    const LLVector3 centre = camera->getOrigin() + camera->getAtAxis() * distance +
        camera->getUpAxis() * (anchor * distance * tangent);
    position = gAgent.getPosGlobalFromAgent(centre);
    // Right-handed basis. The plan's (-at, +left, +up) has determinant -1.
    rotation = LLQuaternion(-camera->getAtAxis(), -camera->getLeftAxis(), camera->getUpAxis());
    const F32 height = 2.f * fraction * distance * tangent;
    scale = LLVector3(.01f, height * ALColorChartData::ASPECT, height);
}

void ALColorChart::pinHere()
{
    if (!gAgent.getRegion() || gDisconnected)
    {
        return;
    }
    if (!unlit() && mPrim.notNull() && !mPrim->isDead() && mPrim->mDrawable.notNull())
    {
        // Pin the displayed Lit transform, including the final camera update.
        const auto* drawable = mPrim->mDrawable.get();
        mPosition = gAgent.getPosGlobalFromAgent(drawable->getPositionAgent());
        mRotation = drawable->getWorldRotation();
        mScale = drawable->getScale();
    }
    else
    {
        cameraTransform(mPosition, mRotation, mScale);
    }
    mPinned = true;
    gSavedSettings.setS32("AlchemyColorChartPlacement", 1); // ui-draw-lint: allow(setting-write) User pin action or one-time initialisation of a requested pin, never an unconditional tick write.
}

void ALColorChart::buildTexture()
{
    using namespace ALColorChartData;
    LLPointer<LLImageRaw> raw = new LLImageRaw(WIDTH, HEIGHT, 3);
    memset(raw->getData(), 0, WIDTH * HEIGHT * 3);
    for (const auto& patch : PATCHES)
    {
        for (S32 y = patch.rect[1]; y < patch.rect[1] + patch.rect[3]; ++y)
        {
            for (S32 x = patch.rect[0]; x < patch.rect[0] + patch.rect[2]; ++x)
            {
                memcpy(raw->getData() + (y * WIDTH + x) * 3, patch.srgb, 3);
            }
        }
    }
    // Legacy diffuse textures are sRGB codes, decoded by the normal material path.
    mTexture = LLViewerTextureManager::getLocalTexture(raw.get(), false);
}

void ALColorChart::idle()
{
    auto* region = gAgent.getRegion();
    if (!region || gDisconnected || LLApp::isExiting())
    {
        releaseResources();
        return;
    }
    static LLCachedControl<S32> placement(gSavedSettings, "AlchemyColorChartPlacement", 0);
    if (placement() == 1 && !mPinned)
    {
        pinHere();
    }
    if (mPinned)
    {
        const LLVector3d camera = gAgent.getPosGlobalFromAgent(LLViewerCamera::getInstance()->getOrigin());
        // Region-crossing is checked in global doubles; stale region pointers are never dereferenced.
        if ((camera - mPosition).length() > 512. ||
            (mCheckRegionPin && !region->pointInRegionGlobal(mPosition)))
        {
            mPinned = false;
            gSavedSettings.setS32("AlchemyColorChartPlacement", 0); // ui-draw-lint: allow(setting-write) One-shot invalid-pin lifecycle transition after teleport or leaving its reach; the flag is cleared immediately.
            LLNotificationsUtil::add("ColorChartReattached");
        }
    }
    mCheckRegionPin = false;
    if (unlit())
    {
        destroyPrim();
        return;
    }
    if (mPrim.notNull() && (mPrim->isDead() || mPrim->getRegion() != region))
    {
        destroyPrim();
    }
    if (mPrim.isNull())
    {
        LLViewerObject* object = gObjectList.createObjectViewer(LL_PCODE_VOLUME, region);
        auto* volume = dynamic_cast<LLVOVolume*>(object);
        if (!volume)
        {
            if (object)
            {
                object->markDead();
            }
            return;
        }
        volume->mbCanSelect = false;
        volume->mIsLocalOnly = true;
        volume->mLocalObjectKind = LLViewerObject::LOCAL_OBJECT_COLOR_CHART;
        gPipeline.createObject(volume);
        volume->setLOD(LLVolumeLODGroup::NUM_LODS - 1);
        LLVolumeParams params;
        params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
        params.setBeginAndEndS(0.f, 1.f);
        params.setBeginAndEndT(0.f, 1.f);
        params.setRatio(1.f, 1.f);
        params.setShear(0.f, 0.f);
        if (!volume->setVolume(params, LLVolumeLODGroup::NUM_LODS - 1, true))
        {
            volume->markDead();
            return;
        }
        volume->setIsLight(false);
        if (mTexture.isNull())
        {
            buildTexture();
        }
        for (U8 face = 0; face < volume->getNumTEs(); ++face)
        {
            const auto& geometry = volume->getVolume()->getVolumeFace(face);
            const bool front = geometry.mNumVertices && geometry.mNormals[0].getF32ptr()[0] > .9f;
            volume->setTEColor(face, front ? LLColor4::white : LLColor4::black);
            volume->setTEImage(face, mTexture);
            volume->setTEFullbright(face, false);
            volume->setTEGlow(face, 0.f);
            volume->setTEShiny(face, 0);
            volume->setTEBumpmap(face, 0);
            volume->setTEMaterialParams(face, LLMaterialPtr());
        }
        mPrim = volume;
    }
    LLVector3d position = mPosition;
    LLQuaternion rotation = mRotation;
    LLVector3 scale = mScale;
    if (!mPinned)
    {
        cameraTransform(position, rotation, scale);
    }
    mPrim->setScale(scale, false);
    mPrim->setRotation(rotation, false);
    mPrim->setPositionGlobal(position, false);
    if (mPrim->mDrawable.notNull())
    {
        // Camera dispatch runs after the ordinary world movement pass.
        gPipeline.updateMoveNormalAsync(mPrim->mDrawable);
    }
}

void ALColorChart::drawChart(LLRenderTarget* dst, const glm::mat4& projection,
    const glm::mat4& modelview, bool exposed, LLRenderTarget* encoded, bool atlas, bool screen_sample)
{
    using namespace ALColorChartData;
    glm::mat4 matrix(1.f);
    if (!atlas)
    {
        if (mPinned)
        {
            const LLVector3 centre = gAgent.getPosAgentFromGlobal(mPosition);
            const LLVector3 horizontal = LLVector3(0.f, mScale.mV[1] * .5f, 0.f) * mRotation;
            const LLVector3 vertical = LLVector3(0.f, 0.f, mScale.mV[2] * .5f) * mRotation;
            matrix[0] = glm::vec4(horizontal.mV[0], horizontal.mV[1], horizontal.mV[2], 0.f);
            matrix[1] = glm::vec4(vertical.mV[0], vertical.mV[1], vertical.mV[2], 0.f);
            matrix[3] = glm::vec4(centre.mV[0], centre.mV[1], centre.mV[2], 1.f);
            matrix = projection * modelview * matrix;
        }
        else
        {
            static LLCachedControl<F32> size(gSavedSettings, "AlchemyColorChartSize", .25f);
            static LLCachedControl<S32> anchor(gSavedSettings, "AlchemyColorChartAnchor", 0);
            const F32 fraction = llclamp(size(), .10f, .60f);
            matrix[0][0] = fraction * ASPECT / LLViewerCamera::getInstance()->getAspect();
            matrix[1][1] = fraction;
            matrix[3][1] = anchor() == 1 ? -.45f : 0.f;
        }
        mLastMatrix = matrix;
        mLastDrawFrame = LLFrameTimer::getFrameCount();
    }
    if (dst)
    {
        dst->bindTarget();
    }
    else
    {
        const auto& rect = gViewerWindow->getWorldViewRectRaw();
        glViewport(rect.mLeft, rect.mBottom, rect.getWidth(), rect.getHeight());
    }
    {
        LLGLDepthTest depth(GL_FALSE, GL_FALSE);
        LLGLDisable blend(GL_BLEND);
        LLGLDisable cull(GL_CULL_FACE);
        auto& shader = gColorChartProgram;
        shader.bind();
        shader.uniformMatrix4fv(MATRIX, 1, GL_FALSE, glm::value_ptr(matrix));
        shader.uniform1i(SAMPLE, screen_sample ? 2 : encoded ? 1 : 0);
        shader.uniform1i(EXPOSED, exposed ? 1 : 0);
        if (screen_sample)
        {
            const auto& rect = gViewerWindow->getWorldViewRectRaw();
            shader.uniform4f(VIEWPORT, static_cast<F32>(rect.mLeft), static_cast<F32>(rect.mBottom),
                             static_cast<F32>(rect.getWidth()), static_cast<F32>(rect.getHeight()));
        }
        S32 input = -1;
        S32 adaptation = -1;
        if (encoded)
        {
            input = shader.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, encoded, false, LLTexUnit::TFO_POINT);
        }
        if (exposed)
        {
            adaptation = shader.bindTexture(LLShaderMgr::EXPOSURE_MAP, &gPipeline.mExposureMap);
            shader.uniform1f(LLShaderMgr::EXPOSURE, llclamp(gSavedSettings.getF32("RenderExposure"), .5f, 4.f));
            shader.uniform1i(LLShaderMgr::TONEMAP_TYPE, gSavedSettings.getS32("AlchemyRenderTonemapType"));
        }
        const auto quad = [](F32 x, F32 y, F32 width, F32 height)
        {
            gGL.begin(LLRender::TRIANGLES);
            gGL.vertex3f(x, y, 0.f);
            gGL.vertex3f(x + width, y, 0.f);
            gGL.vertex3f(x + width, y + height, 0.f);
            gGL.vertex3f(x, y, 0.f);
            gGL.vertex3f(x + width, y + height, 0.f);
            gGL.vertex3f(x, y + height, 0.f);
            gGL.end();
            gGL.flush();
        };
        shader.uniform3f(COLOUR, 0.f, 0.f, 0.f);
        // Black border/gutters, or a single encoded atlas composite.
        quad(-1.f, -1.f, 2.f, 2.f);
        if (!encoded)
        {
            for (const auto& patch : PATCHES)
            {
                const F32 x = 2.f * static_cast<F32>(patch.rect[0]) / static_cast<F32>(WIDTH) - 1.f;
                const F32 y = 2.f * static_cast<F32>(patch.rect[1]) / static_cast<F32>(HEIGHT) - 1.f;
                const F32 width = 2.f * static_cast<F32>(patch.rect[2]) / static_cast<F32>(WIDTH);
                const F32 height = 2.f * static_cast<F32>(patch.rect[3]) / static_cast<F32>(HEIGHT);
                shader.uniform3fv(COLOUR, 1, patch.linear);
                quad(x, y, width, height);
            }
        }
        if (input >= 0)
        {
            shader.unbindTexture(LLShaderMgr::DEFERRED_DIFFUSE);
        }
        if (adaptation >= 0)
        {
            shader.unbindTexture(LLShaderMgr::EXPOSURE_MAP);
        }
        shader.unbind();
    }
    if (dst)
    {
        dst->flush();
    }
}

void ALColorChart::renderUnlit(LLRenderTarget* dst, bool exposed,
    const glm::mat4& projection, const glm::mat4& modelview)
{
    if (gDisconnected || !unlit() || !ensureShaders(false))
    {
        return;
    }
    mLastSensor = exposed;
    drawChart(dst, projection, modelview, exposed);
}

void ALColorChart::renderLegacy(LLRenderTarget* dst, const glm::mat4& projection, const glm::mat4& modelview)
{
    if (gDisconnected || !unlit() || !ensureShaders(true) || mAtlasFailed)
    {
        return;
    }
    // Legacy encodes BEFORE optics. Encode a separate float chart with the
    // production Exposure + Camera stages, using optional private programs.
    if (!mLinear.isComplete())
    {
        if (!mLinear.allocate(ALColorChartData::WIDTH, ALColorChartData::HEIGHT, GL_RGBA32F, false) ||
            !mExposed.allocate(ALColorChartData::WIDTH, ALColorChartData::HEIGHT, GL_RGBA32F, false))
        {
            mLinear.release();
            mExposed.release();
            mAtlasFailed = true;
            LL_WARNS("ColorChart") << "Chart atlas allocation failed; re-enable chart or reload shaders to retry."
                << LL_ENDL;
            return;
        }
    }
    drawChart(&mLinear, glm::mat4(1.f), glm::mat4(1.f), false, nullptr, true);
    gPipeline.colorCorrect(&mLinear, &mExposed, true, false, LLPipeline::SensorStage::Exposure,
                          &gColorChartExposureProgram);
    const U32 permutation = gPipeline.colorCorrectPermutation(true, true);
    gPipeline.colorCorrect(&mExposed, &mLinear, true, true, LLPipeline::SensorStage::Camera,
                          &gColorChartCameraProgram[permutation % 4]);
    mLastSensor = false;
    drawChart(dst, projection, modelview, false, &mLinear);
}

void ALColorChart::presentUnlit(LLRenderTarget* src, bool sensor,
    const glm::mat4& projection, const glm::mat4& modelview)
{
    if (gDisconnected || !unlit())
    {
        return;
    }
    if (sensor)
    {
        if (!ensureShaders(false))
        {
            return;
        }
        // Restore from post-sensor codes, bypassing only Present's artistic
        // vignette/grain/CVD/preview. Sensor noise and purity stay intact.
        drawChart(nullptr, projection, modelview, false, src, false, true);
    }
    else
    {
        renderLegacy(nullptr, projection, modelview);
    }
}

void ALColorChart::debugReadback()
{
    if (gDisconnected || !unlit() || mLastDrawFrame != LLFrameTimer::getFrameCount())
    {
        return;
    }
    if (++mReadbackFrame % 60 != 1)
    {
        return;
    }
    GLint draw_framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
    if (draw_framebuffer != 0)
    {
        LL_INFOS("ColorChart") << "INCONCLUSIVE offscreen/snapshot presentation; window backbuffer not sampled"
            << LL_ENDL;
        return;
    }
    const auto& rect = gViewerWindow->getWorldViewRectRaw();
    ScopedReadbackState saved_state;
    for (const auto& patch : ALColorChartData::PATCHES)
    {
        const F32 patch_x = static_cast<F32>(patch.rect[0]) + static_cast<F32>(patch.rect[2]) * .5f;
        const F32 patch_y = static_cast<F32>(patch.rect[1]) + static_cast<F32>(patch.rect[3]) * .5f;
        const glm::vec4 clip = mLastMatrix * glm::vec4(
            2.f * patch_x / static_cast<F32>(ALColorChartData::WIDTH) - 1.f,
            2.f * patch_y / static_cast<F32>(ALColorChartData::HEIGHT) - 1.f, 0.f, 1.f);
        if (clip.w <= 0.f || fabsf(clip.x) >= clip.w || fabsf(clip.y) >= clip.w || fabsf(clip.z) >= clip.w)
        {
            LL_INFOS("ColorChart") << "INCONCLUSIVE " << patch.name << " centre outside camera clip volume"
                << LL_ENDL;
            continue;
        }
        const F32 window_x = (clip.x / clip.w * .5f + .5f) * static_cast<F32>(rect.getWidth());
        const F32 window_y = (clip.y / clip.w * .5f + .5f) * static_cast<F32>(rect.getHeight());
        F32 pixel[4] = {};
        glReadPixels(rect.mLeft + static_cast<S32>(window_x), rect.mBottom + static_cast<S32>(window_y),
                     1, 1, GL_RGBA, GL_FLOAT, pixel);
        // Independent double Sony model for documented S-Log3/SG3 and Rec.709
        // capture settings. Other curves/gamuts still log raw codes.
        F64 expected[3] = {};
        const S32 gamut = gSavedSettings.getS32("AlchemyToneMapSLogGamut");
        const bool supported = gSavedSettings.getS32("AlchemyRenderTonemapType") == 8 &&
            gSavedSettings.getS32("AlchemyToneMapSLogCurve") == 0 && gamut >= 0 && gamut <= 1;
        if (supported)
        {
            const F64 iris = llclamp(gSavedSettings.getF32("RenderExposure"), .5f, 4.f);
            const F64 ev = exp2(llclamp(gSavedSettings.getF32("AlchemyToneMapSLogEV"), -8.f, 8.f));
            const F64 purity = llclamp(gSavedSettings.getF32("AlchemyToneMapSLogPurity"), .6f, 1.f);
            F64 rgb[3] = {
                patch.linear[0] * iris * ev,
                patch.linear[1] * iris * ev,
                patch.linear[2] * iris * ev };
            const F64 luminance = rgb[0] * .2126 + rgb[1] * .7152 + rgb[2] * .0722;
            if (rgb[0] != rgb[1] || rgb[1] != rgb[2])
            {
                for (F64& channel : rgb)
                {
                    channel = luminance + purity * (channel - luminance);
                }
            }
            constexpr F64 sg3[3][3] = {
                {.5660490800, .3427630874, .0911878326},
                {.0769613789, .7990545001, .1239841211},
                {.0223509133, .1086120512, .8690370356} };
            const F64 toe = .18 * llclamp(gSavedSettings.getF32("AlchemyToneMapSLogToe"), 0.f, .3f);
            for (S32 channel = 0; channel < 3; ++channel)
            {
                F64 value = gamut == 0 ? rgb[channel] :
                    sg3[channel][0] * rgb[0] + sg3[channel][1] * rgb[1] + sg3[channel][2] * rgb[2];
                value = llmax(value, 0.) + toe;
                F64 code = value >= .01125 ? (420. + 261.5 * log10((value + .01) / .19)) / 1023. :
                    (95. + value * (171.2102946929 - 95.) / .01125) / 1023.;
                code = llclamp(code, 0., 1.);
                expected[channel] = gSavedSettings.getS32("AlchemyToneMapSLogRange") == 1 ?
                    (code * 876. + 64.) / 1023. : code;
            }
        }
        const bool noisy = mLastSensor && gSavedSettings.getBOOL("CineSensorNoiseEnabled");
        const F64 tolerance = .25 / 1023. + .5 / (LLRender::s10bitBackBuffer ? 1023. : 255.);
        F64 error = 0.;
        bool finite = true;
        for (S32 channel = 0; channel < 3; ++channel)
        {
            finite = finite && std::isfinite(pixel[channel]);
            error = llmax(error, fabs(static_cast<F64>(pixel[channel]) - expected[channel]));
        }
        const char* verdict = !supported || noisy || !finite ? "INCONCLUSIVE " :
            error <= tolerance ? "PASS " : "FAIL ";
        LL_INFOS("ColorChart") << verdict << (mLastSensor ? "Sensor " : "Legacy ") << patch.name
            << " code=" << pixel[0] << "," << pixel[1] << "," << pixel[2]
            << " expected=" << (supported ? "Sony S-Log3 model " : "unsupported settings ")
            << expected[0] << "," << expected[1] << "," << expected[2]
            << " error=" << pixel[0] - expected[0] << "," << pixel[1] - expected[1] << "," << pixel[2] - expected[2]
            << " tolerance=" << tolerance << " noise=" << noisy << " finite=" << finite
            << " exposure=" << gSavedSettings.getF32("RenderExposure")
            << " EV=" << gSavedSettings.getF32("AlchemyToneMapSLogEV") << LL_ENDL;
    }
}
