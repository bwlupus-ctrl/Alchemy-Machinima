/**
 * @file alweatherparticles.cpp
 * @brief Compute-evaluated world cells and instanced soft snow for every view.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork. Slipstream Viewer contributors: precipitation,
 * coverage-preserving flakes and physical visibility design reference.
 * Reimplemented for OpenGL; no Vulkan renderer source or ABI copied.
 * $/LicenseInfo$
 */
#include "llviewerprecompiledheaders.h"
#include "alweatherparticles.h"
#include "alweatherbridge.h"
#include "alweatherresources.h"
#include "llshadermgr.h"
#include "llvertexbuffer.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

bool ALWeatherParticles::supported()
{
#if LL_DARWIN
    return false;
#else
    return LLShaderMgr::computeShadersSupported() && glDrawArraysInstanced &&
        glVertexAttribDivisor && glGenVertexArrays && glBindVertexArray &&
        LLRender::indexedDrawBufferGuardSupported() && gGLManager.mNumTextureImageUnits >= 2;
#endif
}

void ALWeatherParticles::release()
{
    mSimulation.unload(); mDraw.unload(); mVeil.unload();
    if (mBuffer) glDeleteBuffers(1, &mBuffer);
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    mBuffer = mVAO = 0; mCapacity = 0;
    mAttempted = mReady = mVeilActive = false;
    mStatus = "Falling snow off";
}

bool ALWeatherParticles::initialize(U32 cap)
{
    if (!mAttempted)
    {
        mAttempted = true;
        if (!supported())
        {
            mStatus = "Unavailable: requires OpenGL 4.3 compute, SSBO, images and barriers";
            return false;
        }
        const auto load = [](LLGLSLShader& shader, const char* name, bool compute, bool veil)
        {
            shader.mName = name; shader.mShaderLevel = 1;
            shader.mFeatures = LLShaderFeatures();
            shader.mFeatures.attachNothing = true; // These stages are self-contained.
            shader.mShaderFiles.clear(); shader.clearPermutations();
            if (veil) shader.addPermutation("SNOW_VEIL", "1");
            if (compute) shader.mShaderFiles.emplace_back("deferred/weatherSnowSim.glsl", LLGLSLShader::COMPUTE_STAGE);
            else
            {
                shader.mShaderFiles.emplace_back("deferred/weatherSnowV.glsl", GL_VERTEX_SHADER);
                shader.mShaderFiles.emplace_back(veil ? "deferred/weatherPrecipVeilF.glsl" : "deferred/weatherSnowF.glsl", GL_FRAGMENT_SHADER);
                shader.mShaderFiles.emplace_back("deferred/weatherPrecipCoverF.glsl", GL_FRAGMENT_SHADER);
            }
            return shader.createShader();
        };
        mReady = load(mSimulation, "Weather Snow Cells", true, false) &&
            load(mDraw, "Weather Snow Flakes", false, false) &&
            load(mVeil, "Weather Precipitation Visibility", false, true);
        if (!mReady)
        {
            mSimulation.unload(); mDraw.unload(); mVeil.unload();
            mStatus = "Unavailable: snowfall shader link failed (existing rain available)";
            return false;
        }
        glGenVertexArrays(1, &mVAO);
        if (!mVAO)
        {
            mReady = false;
            mStatus = "Unavailable: snowfall vertex array allocation failed";
            return false;
        }
    }
    if (!mReady) return false;
#if !LL_DARWIN
    if (cap != mCapacity)
    {
        if (mBuffer)
        {
            // An unbound VAO retains its attribute-buffer references after
            // glDeleteBuffers. Drop it too, including snow -> veil-only.
            glDeleteVertexArrays(1, &mVAO); mVAO = 0;
            glDeleteBuffers(1, &mBuffer);
            glGenVertexArrays(1, &mVAO);
        }
        mBuffer = 0; mCapacity = 0;
        if (!mVAO)
        {
            mReady = false;
            mStatus = "Unavailable: snowfall vertex array allocation failed";
            return false;
        }
        if (cap)
        {
            glGenBuffers(1, &mBuffer);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, cap * 32U, nullptr, GL_DYNAMIC_DRAW);
            GLint actual = 0;
            glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &actual);
            if (actual != GLint(cap * 32U))
            {
                glDeleteBuffers(1, &mBuffer); mBuffer = 0; mReady = false;
                mStatus = "Unavailable: snowfall buffer allocation failed";
                return false;
            }
            mCapacity = cap;
        }
    }
#endif
    return mVAO != 0;
}

void ALWeatherParticles::render(LLRenderTarget& target, LLRenderTarget& depth,
    const glm::mat4& view, const glm::mat4& projection, const glm::dvec3& origin,
    const glm::vec3& light, ALWeatherResources& cover, U32 frame)
{
    const auto& weather = ALWeatherBridge::Controller::instance();
    if (!weather.precipitationActive())
    {
        if (allocated()) release();
        return;
    }
#if !LL_DARWIN
    if (!supported())
    {
        if (allocated()) release();
        mStatus = "Unavailable: requires OpenGL 4.3 compute, SSBO, images and barriers";
        return;
    }
    const auto& s = weather.settings();
    // Construct hashed names only on first enabled use; steady draws and the
    // off path do not allocate temporary uniform-name strings.
    static const LLStaticHashedString sInverseProjection("snow_inverse_projection"), sLight("snow_light"),
        sVeilLight("snow_veil_light"), sExtinction("snow_extinction"), sInverseView("snow_inverse_view"),
        sCount("snow_count"), sDimension("snow_dimension"), sSlots("snow_slots"), sSeed("snow_seed"),
        sCellOrigin("snow_cell_origin"), sCellRelative("snow_cell_relative"), sAmount("snow_amount"),
        sTime("snow_time"), sView("snow_view"), sProjection("snow_projection"), sScreen("snow_screen"),
        sSize("snow_size"), sRange("snow_range");
    // Preserve bindings not owned by the shared raster state transaction.
    ALWeatherResources::State saved_state;
    LLGLSLShader* previous = LLGLSLShader::sCurBoundShaderPtr;
    const U32 attribute_mask = LLVertexBuffer::sLastMask;
    const U32 array_cache = LLVertexBuffer::sGLRenderBuffer, element_cache = LLVertexBuffer::sGLRenderIndices;
    GLint vao = 0, array = 0, element = 0, storage = 0, indexed = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element);
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &storage);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &indexed);
    LLVertexBuffer::unbind();
    // A local scope restores even allocation/link failure exits.
    const auto restore = [&]()
    {
        LLGLSLShader::unbind();
        glBindVertexArray(vao);
        // Shader binding updates a single native attribute cache. A private
        // VAO must restore both the original enables and that cache together.
        LLVertexBuffer::sLastMask = ~attribute_mask;
        LLVertexBuffer::setupClientArrays(attribute_mask);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, indexed);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, storage);
        if (previous) previous->bind();
        glBindBuffer(GL_ARRAY_BUFFER, array); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element);
        LLVertexBuffer::sGLRenderBuffer = array_cache;
        LLVertexBuffer::sGLRenderIndices = element_cache;
    };
    mVeilActive = false;
    if (!initialize(s.snowEnabled ? s.snowLimit : 0)) { restore(); return; }
    glBindVertexArray(mVAO);
    LLVertexBuffer::sLastMask = ~U32(0);
    LLVertexBuffer::setupClientArrays(0);
    mVeilActive = s.visibility && s.visibilityAmount > 0.f;
    const F32 extinction = mVeilActive ? SLWeather::precipExtinction(
        weather.state().rainFall, weather.state().snowFall, 0.f) * s.visibilityAmount : 0.f;
    const glm::mat4 inverse_view = glm::inverse(view), inverse_projection = glm::inverse(projection);
    const glm::dvec3 eye = origin + glm::dvec3(inverse_view[3]);
    mStatus = cover.precipitationCovers(glm::vec3(inverse_view[3]), origin, frame) ?
        "Snow ready (world cells; bounded GPU list)" :
        "Snow ready; insufficient cover for this view (prepare its recording area)";
    target.bindTarget();
    LLGLEnable blend(GL_BLEND);
    LLGLDisable cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    gGL.setColorMask(true, false);
    gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_ONE_MINUS_SOURCE_ALPHA);
    const auto bind = [&](LLGLSLShader& shader)
    {
        shader.bind();
        shader.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &depth, true, LLTexUnit::TFO_POINT);
        cover.bindPrecipitation(&shader, origin, frame);
        shader.uniformMatrix4fv(sInverseProjection, 1, false, glm::value_ptr(inverse_projection));
        shader.uniform3f(sLight, light.x, light.y, light.z);
        shader.uniform3f(sVeilLight, light.x * 0.8f, light.y * 0.8f, light.z * 0.8f);
        shader.uniform1f(sExtinction, extinction);
    };
    if (extinction > 0.f)
    {
        LLGLDepthTest no_depth(GL_FALSE, GL_FALSE);
        bind(mVeil);
        mVeil.uniformMatrix4fv(sInverseView, 1, false, glm::value_ptr(inverse_view));
        glBindVertexArray(mVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3); // One extinction application per target view.
        LLGLSLShader::unbind();
    }
    if (s.snowEnabled && ALWeatherBridge::snowAmount(weather.state()) > 0.f)
    {
        const U32 dimension = 2U * U32(std::ceil(s.snowRange / 32.f)) + 3U;
        const U32 cells = dimension * dimension * dimension;
        const U32 slots = std::max(1U, s.snowLimit / cells);
        const U32 count = std::min(s.snowLimit, cells * slots);
        const auto& drift = weather.snowDrift();
        const glm::dvec3 displacement(drift[0], drift[1], drift[2]);
        const glm::dvec3 base = glm::floor((eye-displacement)/32.0) - glm::dvec3(dimension/2);
        const glm::vec3 relative(base*32.0+displacement-origin);
        mSimulation.bind();
        mSimulation.uniform1i(sCount, count);
        mSimulation.uniform1i(sDimension, dimension);
        mSimulation.uniform1i(sSlots, slots);
        // Uniform uint upload uses the existing hash lookup; no float seed loss.
        glUniform1ui(mSimulation.getUniformLocation(sSeed), s.seed);
        glUniform3i(mSimulation.getUniformLocation(sCellOrigin), GLint(base.x), GLint(base.y), GLint(base.z));
        mSimulation.uniform3f(sCellRelative, relative.x, relative.y, relative.z);
        mSimulation.uniform1f(sAmount, ALWeatherBridge::snowAmount(weather.state()));
        mSimulation.uniform1f(sTime, F32(weather.clock().seconds()));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mBuffer);
        // Reuse waits for the preceding view's vertex fetches, then publish
        // below for this view. ARB_shader_storage_buffer_object, memory access:
        // https://registry.khronos.org/OpenGL/extensions/ARB/ARB_shader_storage_buffer_object.txt
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute((count+127U)/128U, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
        LLGLSLShader::unbind();
        LLGLDepthTest depth_test(GL_TRUE, GL_FALSE, GL_LEQUAL);
        bind(mDraw);
        mDraw.uniformMatrix4fv(sView, 1, false, glm::value_ptr(view));
        mDraw.uniformMatrix4fv(sProjection, 1, false, glm::value_ptr(projection));
        mDraw.uniform2f(sScreen, F32(target.getWidth()), F32(target.getHeight()));
        mDraw.uniform1f(sSize, s.flakeSize);
        mDraw.uniform1f(sRange, s.snowRange);
        glBindVertexArray(mVAO); glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
        const GLuint position = glGetAttribLocation(mDraw.mProgramObject, "position");
        const GLuint traits = glGetAttribLocation(mDraw.mProgramObject, "texcoord0");
        glEnableVertexAttribArray(position); glEnableVertexAttribArray(traits);
        glVertexAttribPointer(position, 4, GL_FLOAT, GL_FALSE, 32, nullptr);
        glVertexAttribPointer(traits, 4, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(16));
        glVertexAttribDivisor(position, 1); glVertexAttribDivisor(traits, 1);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
    }
    target.flush();
    restore();
#else
    mStatus = "Unavailable: requires OpenGL 4.3 compute, SSBO, images and barriers";
#endif
}
