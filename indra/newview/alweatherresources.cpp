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

ALWeatherResources::State::State()
{
    gGL.flush();
    mActive = gGL.getCurrentTexUnitIndex();
    mUnits = std::min(U32(gGLManager.mNumTextureImageUnits), U32(mTextures.size()));
    for (U32 i = 0; i < mUnits; ++i)
    {
        const auto unit = gGL.getTexUnit(i);
        mTextures[i] = {unit->getCurrType(), unit->getCurrTexture(), unit->hasMipMaps()};
    }
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &mDrawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFBO);
    glGetIntegerv(GL_READ_BUFFER, &mReadBuffer);
    glGetIntegerv(GL_CULL_FACE_MODE, &mCull);
    glGetIntegerv(GL_FRONT_FACE, &mFront);
    glGetIntegerv(GL_VIEWPORT, mViewport);
    glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &mClearDepth);
    GLint buffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &buffers);
    mBuffers = std::min(U32(buffers), U32(mDraw.size()));
    gGL.getBlendFunc(mCS, mCD, mAS, mAD);
    for (U32 i = 0; i < mBuffers; ++i)
    {
        auto& d = mDraw[i];
        glGetIntegerv(GL_DRAW_BUFFER0 + i, &d.buffer);
        glGetBooleani_v(GL_COLOR_WRITEMASK, i, d.mask);
        d.blend = glIsEnabledi(GL_BLEND, i);
        glGetIntegeri_v(GL_BLEND_SRC_RGB, i, &d.src);
        glGetIntegeri_v(GL_BLEND_DST_RGB, i, &d.dst);
        glGetIntegeri_v(GL_BLEND_SRC_ALPHA, i, &d.srcAlpha);
        glGetIntegeri_v(GL_BLEND_DST_ALPHA, i, &d.dstAlpha);
        glGetIntegeri_v(GL_BLEND_EQUATION_RGB, i, &d.eq);
        glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA, i, &d.eqAlpha);
    }
}
ALWeatherResources::State::~State()
{
    gGL.flush();
    // Restore caches through their owners, then restore any indexed overrides.
    for (U32 i = 0; i < mUnits; ++i)
    {
        const auto& t = mTextures[i];
        if (t.type == LLTexUnit::TT_NONE) gGL.getTexUnit(i)->disable();
        else gGL.getTexUnit(i)->bindManual(t.type, t.id != 0 && !glIsTexture(t.id) ? 0 : t.id, t.mips);
    }
    gGL.getTexUnit(mActive)->activate();
    gGL.blendFunc(mCS, mCD, mAS, mAD);
    gGL.setColorMask(mDraw[0].mask[0], mDraw[0].mask[1], mDraw[0].mask[2], mDraw[0].mask[3]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mReadFBO);
    std::array<GLenum, 32> buffers{};
    for (U32 i = 0; i < mBuffers; ++i) buffers[i] = GLenum(mDraw[i].buffer);
    // The default framebuffer accepts a single GL_BACK draw buffer.
    glDrawBuffers(mDrawFBO ? mBuffers : 1, buffers.data());
    glReadBuffer(mReadBuffer);
    glCullFace(mCull);
    glFrontFace(mFront);
    glDepthFunc(mDepthFunc);
    glDepthMask(mDepthMask);
    glClearDepth(mClearDepth);
    glViewport(mViewport[0], mViewport[1], mViewport[2], mViewport[3]);
    for (U32 i = 0; i < mBuffers; ++i)
    {
        const auto& d = mDraw[i];
        glColorMaski(i, d.mask[0], d.mask[1], d.mask[2], d.mask[3]);
        if (d.blend) glEnablei(GL_BLEND, i); else glDisablei(GL_BLEND, i);
        glBlendFuncSeparatei(i, d.src, d.dst, d.srcAlpha, d.dstAlpha);
        glBlendEquationSeparatei(i, d.eq, d.eqAlpha);
    }
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
    shader->uniform1i(LLShaderMgr::WEATHER_MATERIAL_ENABLED, enabled);
    if (!enabled) return;
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
    if (!readyForFrame(frame) || origin != mOrigin || ALWeatherBridge::Controller::instance().placeSerial() != mPlace) return false;
    const glm::vec4 q = mMatrix * glm::vec4(eye, 1.f);
    return q.w > 0.f && glm::all(glm::lessThan(glm::abs(glm::vec3(q) / q.w), glm::vec3(1.f)));
}

void ALWeatherResources::bindPrecipitation(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame)
{
    static const LLStaticHashedString sCoverValid("snow_cover_valid");
    const bool current = readyForFrame(frame) && origin == mOrigin &&
        ALWeatherBridge::Controller::instance().placeSerial() == mPlace;
    shader->uniform1i(sCoverValid, current ? 1 : 0);
    if (!current) return;
    shader->bindTexture(LLShaderMgr::WEATHER_SURFACE_COVER_MAP, &mCover[mRead], true, LLTexUnit::TFO_POINT);
    shader->uniformMatrix4fv(LLShaderMgr::WEATHER_SURFACE_COVER_MATRIX, 1, false, glm::value_ptr(mMatrix));
}
