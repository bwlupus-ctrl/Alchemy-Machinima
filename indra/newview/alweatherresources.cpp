/**
 * @file alweatherresources.cpp
 * @brief Native GL material resources; Slipstream surface design reimplemented for Alchemy.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "llviewerprecompiledheaders.h"
#include "alweatherresources.h"
#include "llglslshader.h"
#include "llshadermgr.h"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>

ALWeatherResources::State::State(U32 units, U32 buffers, bool full) : mFull(full)
{
    gGL.flush();
    glGetIntegerv(GL_CURRENT_PROGRAM, &mProgramName);
    mCachedProgram = LLGLSLShader::sCurBoundShader;
    mProgram = LLGLSLShader::sCurBoundShaderPtr;
    mActive = gGL.getCurrentTexUnitIndex();
    mUnits = std::min(units, std::min(U32(gGLManager.mNumTextureImageUnits), U32(mTextures.size())));
    for (U32 i = 0; i < mUnits; ++i)
    {
        const auto unit = gGL.getTexUnit(i);
        mTextures[i] = {unit->getCurrType(), unit->getCurrTexture(), unit->hasMipMaps()};
    }
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &mDrawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFBO);
    glGetIntegerv(GL_READ_BUFFER, &mReadBuffer);
    glGetIntegerv(GL_VIEWPORT, mViewport);
    if (mFull)
    {
        glGetIntegerv(GL_CULL_FACE_MODE, &mCull);
        glGetIntegerv(GL_FRONT_FACE, &mFront);
        glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &mClearDepth);
        gGL.getBlendFunc(mCS, mCD, mAS, mAD);
    }
    GLint max_buffers = 0;
    if (mFull)
    {
        static U32 generation = ~U32(0);
        static GLint cached_buffers = 0;
        if (generation != LLShaderMgr::sContextGeneration)
        {
            glGetIntegerv(GL_MAX_DRAW_BUFFERS, &cached_buffers);
            generation = LLShaderMgr::sContextGeneration;
        }
        max_buffers = cached_buffers;
    }
    else max_buffers = GLint(buffers);
    mBuffers = std::min(buffers, std::min(U32(max_buffers), U32(mDraw.size())));
    for (U32 i = 0; i < mBuffers; ++i)
    {
        auto& d = mDraw[i];
        if (mFull) glGetIntegerv(GL_DRAW_BUFFER0 + i, &d.buffer);
        glGetBooleani_v(GL_COLOR_WRITEMASK, i, d.mask);
        d.blend = glIsEnabledi(GL_BLEND, i);
        glGetIntegeri_v(GL_BLEND_SRC_RGB, i, &d.src);
        glGetIntegeri_v(GL_BLEND_DST_RGB, i, &d.dst);
        glGetIntegeri_v(GL_BLEND_SRC_ALPHA, i, &d.srcAlpha);
        glGetIntegeri_v(GL_BLEND_DST_ALPHA, i, &d.dstAlpha);
        if (mFull)
        {
            glGetIntegeri_v(GL_BLEND_EQUATION_RGB, i, &d.eq);
            glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA, i, &d.eqAlpha);
        }
    }
}
ALWeatherResources::State::~State()
{
    gGL.flush();
    // Restore caches through their owners, then restore any indexed overrides.
    for (U32 i = 0; i < mUnits; ++i)
    {
        const auto& t = mTextures[i];
        const auto unit = gGL.getTexUnit(i);
        const U32 texture = t.id != 0 && !glIsTexture(t.id) ? 0 : t.id;
        // Most shadow draws touch only a few units. Preserve untouched cached
        // bindings without activating/rebinding every unit in the context.
        if (unit->getCurrType() == t.type && unit->getCurrTexture() == texture &&
            unit->hasMipMaps() == t.mips) continue;
        if (t.type == LLTexUnit::TT_NONE) unit->disable();
        else unit->bindManual(t.type, texture, t.mips);
    }
    gGL.getTexUnit(mActive)->activate();
    if (mFull)
    {
        gGL.blendFunc(mCS, mCD, mAS, mAD);
        gGL.setColorMask(mDraw[0].mask[0], mDraw[0].mask[1], mDraw[0].mask[2], mDraw[0].mask[3]);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mReadFBO);
    std::array<GLenum, 32> buffers{};
    if (mFull)
        for (U32 i = 0; i < mBuffers; ++i) buffers[i] = GLenum(mDraw[i].buffer);
    // The default framebuffer accepts a single GL_BACK draw buffer.
    if (mFull) glDrawBuffers(mDrawFBO ? mBuffers : 1, buffers.data());
    glReadBuffer(mReadBuffer);
    if (mFull)
    {
        glCullFace(mCull);
        glFrontFace(mFront);
        glDepthFunc(mDepthFunc);
        glDepthMask(mDepthMask);
        glClearDepth(mClearDepth);
    }
    glViewport(mViewport[0], mViewport[1], mViewport[2], mViewport[3]);
    for (U32 i = 0; i < mBuffers; ++i)
    {
        const auto& d = mDraw[i];
        glColorMaski(i, d.mask[0], d.mask[1], d.mask[2], d.mask[3]);
        if (d.blend) glEnablei(GL_BLEND, i); else glDisablei(GL_BLEND, i);
        glBlendFuncSeparatei(i, d.src, d.dst, d.srcAlpha, d.dstAlpha);
        if (mFull) glBlendEquationSeparatei(i, d.eq, d.eqAlpha);
    }
    if (mProgramName == 0 || glIsProgram(mProgramName))
    {
        glUseProgram(mProgramName);
        LLGLSLShader::sCurBoundShader = mCachedProgram;
        LLGLSLShader::sCurBoundShaderPtr = mProgram;
    }
    else LLGLSLShader::unbind();
}

void ALWeatherResources::prepare(U32 resolution)
{
    if (allocationFailed(resolution) ||
        (mCover[0].getWidth() == resolution && mCover[1].getWidth() == resolution)) return;
    State saved_state;
    allocate(resolution);
}
bool ALWeatherResources::allocate(U32 resolution)
{
    if (allocationFailed(resolution)) return false;
    if (mCover[0].getWidth() == resolution && mCover[1].getWidth() == resolution) return true;
    const U32 prepared = mPreparedFrame;
    release();
    mPreparedFrame = prepared;
    for (auto& target : mCover)
    {
        if (!target.allocate(resolution, resolution, 0, true, false, LLTexUnit::TT_TEXTURE,
                             LLTexUnit::TMG_NONE, LLRenderTarget::DEPTH_FMT_32F))
        {
            release();
            mPreparedFrame = prepared;
            mFailedResolution = resolution;
            fail(ALWeatherShelter::Coverage::FAILED);
            return false;
        }
        gGL.getTexUnit(0)->bind(&target, true);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    }
    return true;
}
void ALWeatherResources::release()
{
    for (auto& target : mCover) target.release();
    mCoverage = ALWeatherShelter::Coverage::INCOMPLETE;
    mPreparedFrame = ~U32(0);
    mPublishedFrame = ~U32(0);
    mFailedResolution = 0;
    mStatus = "Persistent wet surfaces off";
}
void ALWeatherResources::fail(ALWeatherShelter::Coverage state, const char* reason)
{
    mCoverage = state;
    mStatus = reason ? reason : ALWeatherShelter::label(state);
}
void ALWeatherResources::publish(const glm::mat4& matrix, F32 range, U32 batches, bool complete,
                                  const glm::dvec3& origin, U32 place)
{
    const F32* values = glm::value_ptr(matrix);
    const bool finite_map = std::all_of(values, values + 16, [](F32 value) { return std::isfinite(value); }) &&
        std::isfinite(range) && range > 0.f && std::isfinite(origin.x) &&
        std::isfinite(origin.y) && std::isfinite(origin.z);
    if (!finite_map)
    {
        fail(ALWeatherShelter::Coverage::FAILED, "Surface cover publication invalid; coverage unknown");
        return;
    }
    fail(ALWeatherShelter::classify(writeTarget().isComplete(), true, true, complete, batches));
    if (!ready()) return;
    mMatrix = matrix;
    mRange = range;
    mOrigin = origin;
    mPlace = place;
    mStatus += " (" + std::to_string(writeTarget().getWidth()) + " px; +/-" +
        std::to_string(U32(ALWeatherBridge::Controller::instance().settings().coverExtent)) + " m; " +
        std::to_string(ALWeatherShelter::coverBytes(writeTarget().getWidth()) / 1024) + " KiB)";
    mRead = 1 - mRead; // Texture and all stamps publish together after flush.
    mPublishedFrame = mPreparedFrame;
}
void ALWeatherResources::bind(LLGLSLShader* shader, const glm::mat4& view_to_agent, const glm::dvec3& origin, U32 frame)
{
    const auto& weather = ALWeatherBridge::Controller::instance();
    const auto& s = weather.settings();
    const bool enabled = weather.surfacesActive() && readyForFrame(frame) && weather.placeSerial() == mPlace && origin == mOrigin;
    const bool tilted = enabled && s.advancedShelter && advanced;
    shader->uniform1i(LLShaderMgr::WEATHER_MATERIAL_ENABLED,
        tilted ? (advanced->current(origin, frame) ? 2 : 0) : enabled);
    if (!enabled) return;
    if (tilted) advanced->bind(shader, origin, frame);
    shader->bindTexture(LLShaderMgr::WEATHER_SURFACE_COVER_MAP, &mCover[mRead], true, LLTexUnit::TFO_POINT);
    shader->uniformMatrix4fv(LLShaderMgr::WEATHER_SURFACE_COVER_MATRIX, 1, GL_FALSE, glm::value_ptr(mMatrix));
    shader->uniformMatrix4fv(LLShaderMgr::WEATHER_MATERIAL_VIEW_TO_AGENT, 1, GL_FALSE, glm::value_ptr(view_to_agent));
    // Periodic global coordinates retain phase across region-origin rebases and camera motion.
    shader->uniform3f(LLShaderMgr::WEATHER_MATERIAL_ORIGIN,
        F32(std::fmod(origin.x, 4096.)), F32(std::fmod(origin.y, 4096.)), F32(std::fmod(origin.z, 4096.)));
    shader->uniform4f(LLShaderMgr::WEATHER_MATERIAL_WET,
        weather.state().wetness * s.wetAmount, s.puddles ? s.puddleAmount * weather.state().wetness : 0.f,
        s.rippleStrength, weather.state().rain);
    shader->uniform2f(LLShaderMgr::WEATHER_MATERIAL_TIME_RANGE,
        F32(std::fmod(weather.clock().seconds(), 4096.)), mRange);
}

bool ALWeatherResources::precipitationCovers(const glm::vec3& eye, const glm::dvec3& origin, U32 frame) const
{
    if (ALWeatherBridge::Controller::instance().settings().advancedShelter && advanced)
        return advanced->covers(eye, origin, frame);
    if (!readyForFrame(frame) || origin != mOrigin || ALWeatherBridge::Controller::instance().placeSerial() != mPlace) return false;
    const glm::vec4 q = mMatrix * glm::vec4(eye, 1.f);
    return q.w > 0.f && glm::all(glm::lessThan(glm::abs(glm::vec3(q) / q.w), glm::vec3(1.f)));
}

void ALWeatherResources::bindPrecipitation(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame)
{
    static const LLStaticHashedString sCoverValid("snow_cover_valid");
    const bool current = readyForFrame(frame) && origin == mOrigin &&
        ALWeatherBridge::Controller::instance().placeSerial() == mPlace;
    const bool tilted = ALWeatherBridge::Controller::instance().settings().advancedShelter && advanced;
    shader->uniform1i(sCoverValid, tilted ? (advanced->current(origin, frame) ? 2 : 0) : (current ? 1 : 0));
    if (tilted) advanced->bind(shader, origin, frame);
    if (!current) return;
    shader->bindTexture(LLShaderMgr::WEATHER_SURFACE_COVER_MAP, &mCover[mRead], true, LLTexUnit::TFO_POINT);
    shader->uniformMatrix4fv(LLShaderMgr::WEATHER_SURFACE_COVER_MATRIX, 1, false, glm::value_ptr(mMatrix));
}

void ALWeatherResources::bindMap(LLGLSLShader* shader, S32 sampler, const LLStaticHashedString& matrix)
{
    shader->bindTexture(sampler, &mCover[mRead], true, LLTexUnit::TFO_POINT);
    shader->uniformMatrix4fv(matrix, 1, false, glm::value_ptr(mMatrix));
}

void ALWeatherShelterMaps::prepare()
{
    const auto& s = ALWeatherBridge::Controller::instance().settings();
    if (!s.enabled || !s.advancedShelter) return;
    const U32 generation = LLShaderMgr::sContextGeneration;
    if (mAttemptGeneration == generation && mResolution[0] == s.shelterNearResolution &&
        mResolution[1] == s.shelterFarResolution) return;
    mAttemptGeneration = generation;
    mResolution[0] = s.shelterNearResolution; mResolution[1] = s.shelterFarResolution;
    mReady = false;
    updates.invalidate();
    // Both read/write sides count. Unsupported requested detail is reported,
    // never silently lowered during capture.
    const U64 bytes = ALWeatherShelter::advancedBytes(mResolution[0], mResolution[1]);
    if (bytes > 16ULL * 1024 * 1024)
    {
        fail("Advanced shelter unknown: requested detail exceeds 16 MiB; reduce Near/Far detail");
        return;
    }
    bool barriers = false;
#if !LL_DARWIN
    barriers = glMemoryBarrier != nullptr;
#endif
    if (!LLRender::indexedDrawBufferGuardSupported() || gGLManager.mNumTextureImageUnits < 4 ||
        gGLManager.mGLVersion < 4.19f || !barriers)
    {
        fail("Advanced shelter unavailable: indexed GL state or sampler support missing; existing fallback");
        return;
    }
    ALWeatherResources::State saved_state;
    if (!layers[0].allocate(mResolution[0]) || !layers[1].allocate(mResolution[1]))
    {
        fail("Advanced shelter allocation failed; coverage unknown, existing fallback");
        return;
    }
    if (!mRain.isComplete())
    {
        mRain.mName = "Weather Advanced Shelter Rain";
        mRain.mShaderLevel = 1;
        mRain.mFeatures = LLShaderFeatures();
        mRain.mFeatures.isDeferred = true;
        mRain.mShaderFiles.clear(); mRain.clearPermutations();
        mRain.addPermutation("WEATHER_RAIN_OCCLUSION", "1");
        mRain.addPermutation("WEATHER_ADVANCED_SHELTER", "1");
        mRain.mShaderFiles.emplace_back("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER);
        mRain.mShaderFiles.emplace_back("deferred/weatherAdvancedRainF.glsl", GL_FRAGMENT_SHADER);
        mRain.mShaderFiles.emplace_back("deferred/weatherShelterF.glsl", GL_FRAGMENT_SHADER);
        if (!mRain.createShader())
        {
            fail("Advanced shelter shader failed; coverage unknown, existing fallback");
            return;
        }
    }
    mReady = true;
    mStatus = "Advanced shelter preparing; coverage unknown";
}
void ALWeatherShelterMaps::release()
{
    for (auto& layer : layers) layer.release();
    mRain.unload();
    mReady = false; mAttemptGeneration = ~U32(0);
    mPublishedFrame = preparedFrame = ~U32(0);
    updates.invalidate();
    mStatus = "Advanced shelter off";
}
void ALWeatherShelterMaps::fail(const char* reason)
{
    // Resource failures already clear mReady in prepare(). A failed traversal
    // keeps its session resources and retries at the bounded update cadence.
    mPublishedFrame = ~U32(0);
    updates.invalidate();
    mStatus = reason;
}
bool ALWeatherShelterMaps::current(const glm::dvec3& origin, U32 frame) const
{
    return mReady && mPublishedFrame == frame && origin == mOrigin &&
        mPlace == ALWeatherBridge::Controller::instance().placeSerial() &&
        layers[0].readyForFrame(frame) && layers[1].readyForFrame(frame);
}
bool ALWeatherShelterMaps::covers(const glm::vec3& eye, const glm::dvec3& origin, U32 frame) const
{
    return current(origin, frame) && (layers[0].precipitationCovers(eye, origin, frame) ||
                                     layers[1].precipitationCovers(eye, origin, frame));
}
void ALWeatherShelterMaps::publish(U32 frame)
{
    const auto& weather = ALWeatherBridge::Controller::instance();
    if (!mReady || !layers[0].readyForFrame(frame) || !layers[1].readyForFrame(frame) ||
        layers[0].mOrigin != layers[1].mOrigin || layers[0].mPlace != layers[1].mPlace)
    {
        fail("Advanced shelter update failed; coverage unknown, existing fallback");
        return;
    }
    mOrigin = layers[0].mOrigin;
    mPlace = weather.placeSerial();
    mPublishedFrame = frame;
    updates.publish();
    const auto& s = weather.settings();
    mStatus = "Advanced shelter ready; near +/-" + std::to_string(U32(s.shelterNearExtent)) +
        " m, far +/-" + std::to_string(U32(s.shelterFarExtent)) + " m; update <=" +
        std::to_string(s.shelterHz) + " Hz; outside coverage unknown; blended glass excluded";
}
void ALWeatherShelterMaps::bind(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame)
{
    static const LLStaticHashedString matrices[2] = {LLStaticHashedString("weather_shelter_close_matrix"), LLStaticHashedString("weather_shelter_distant_matrix")};
    static const LLStaticHashedString params("weather_shelter_params");
    if (!current(origin, frame)) return;
    layers[0].bindMap(shader, LLShaderMgr::WEATHER_SHELTER_CLOSE_MAP, matrices[0]);
    layers[1].bindMap(shader, LLShaderMgr::WEATHER_SHELTER_DISTANT_MAP, matrices[1]);
    const auto& s = ALWeatherBridge::Controller::instance().settings();
    shader->uniform4f(params, s.shelterBias, s.shelterSoftness, layers[0].mRange, layers[1].mRange);
    static const LLStaticHashedString debug("weather_shelter_debug");
    shader->uniform1i(debug, s.shelterDebug ? 1 : 0);
}
