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
    return gGLManager.mGLVersion >= 4.49f && glTextureBarrier &&
        LLShaderMgr::computeShadersSupported() && glDrawArraysInstanced &&
        glVertexAttribDivisor && glGenVertexArrays && glBindVertexArray &&
        LLRender::indexedDrawBufferGuardSupported() && gGLManager.mNumTextureImageUnits >= 7;
#endif
}

void ALWeatherParticles::release()
{
    // Programs belong to the central shader inventory; resizing only drops resources.
    mVeilBuffer.release();
    if (mBuffer) glDeleteBuffers(1, &mBuffer);
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    mBuffer = mVAO = 0; mCapacity = 0;
    mAttempted = mReady = mVeilActive = false;
    mStatus = "Falling snow off";
}

void ALWeatherParticles::preloadShaders()
{
    if (!supported())
    {
        mSnowReady = mVeilReady = mTiltedVeilReady = false;
        return;
    }
    const auto load = [](LLGLSLShader& shader, const char* name, bool compute, bool veil, bool tilted = false)
    {
        shader.mName = name; shader.mShaderLevel = 1;
        shader.mFeatures = LLShaderFeatures();
        shader.mFeatures.attachNothing = true; // These stages are self-contained.
        shader.mShaderFiles.clear(); shader.clearPermutations();
        if (veil)
        {
            shader.addPermutation("SNOW_VEIL", "1");
            shader.addPermutation("SNOW_VEIL_BUFFER", "1");
            shader.addPermutation("WEATHER_PARTICLE_ADVANCED", tilted ? "1" : "0");
        }
        if (compute) shader.mShaderFiles.emplace_back("deferred/weatherSnowSim.glsl", LLGLSLShader::COMPUTE_STAGE);
        else
        {
            shader.mShaderFiles.emplace_back("deferred/weatherSnowV.glsl", GL_VERTEX_SHADER);
            shader.mShaderFiles.emplace_back(veil ? "deferred/weatherPrecipVeilF.glsl" : "deferred/weatherSnowF.glsl", GL_FRAGMENT_SHADER);
            shader.mShaderFiles.emplace_back("deferred/weatherPrecipCoverF.glsl", GL_FRAGMENT_SHADER);
            shader.mShaderFiles.emplace_back("deferred/weatherShelterF.glsl", GL_FRAGMENT_SHADER);
        }
        return shader.createShader();
    };
    mSnowReady = mSimulation.isComplete() && mDraw.isComplete();
    if (!mSimulation.isComplete() || !mDraw.isComplete())
    {
        mSnowReady = load(mSimulation, "Weather Snow Cells", true, false) &&
            load(mDraw, "Weather Snow Flakes", false, false);
    }
    if (mSnowReady)
    {
        mPosition = glGetAttribLocation(mDraw.mProgramObject, "position");
        mTraits = glGetAttribLocation(mDraw.mProgramObject, "texcoord0");
        mSnowReady = mPosition >= 0 && mTraits >= 0;
    }
    mTiltedVeilReady = mTiltedVeil.isComplete();
    if (!mTiltedVeil.isComplete())
        mTiltedVeilReady = load(mTiltedVeil, "Weather Tilted Precipitation Visibility", false, true, true);
    mVeilReady = mVeil.isComplete();
    if (!mVeil.isComplete())
        mVeilReady = load(mVeil, "Weather Precipitation Visibility", false, true);
    if (!mVeilComposite.isComplete())
    {
        mVeilComposite.mName = "Weather visibility depth-aware composite";
        mVeilComposite.mShaderLevel = 1;
        mVeilComposite.mFeatures = LLShaderFeatures();
        mVeilComposite.mFeatures.attachNothing = true;
        mVeilComposite.addPermutation("SNOW_VEIL", "1");
        mVeilComposite.mShaderFiles.emplace_back("deferred/weatherSnowV.glsl", GL_VERTEX_SHADER);
        mVeilComposite.mShaderFiles.emplace_back("deferred/weatherPrecipCompositeF.glsl", GL_FRAGMENT_SHADER);
        mVeilComposite.createShader();
    }
}

bool ALWeatherParticles::initialize(U32 cap)
{
    const auto& settings = ALWeatherBridge::Controller::instance().settings();
    const bool veil = settings.visibility && settings.visibilityAmount > 0.f;
    if (!mAttempted || (settings.snowEnabled && !mSnowReady) ||
        (veil && (!(settings.advancedShelter ? mTiltedVeilReady : mVeilReady) || !mVeilBuffer.isComplete())))
    {
        mAttempted = true;
        if (!supported())
        {
            mStatus = "Unavailable: requires OpenGL 4.5 compute and texture barriers";
            return false;
        }
        // A reusable scalar half-resolution field fits the WP3 16 MiB cap at
        // 4K, including its cover maps and particle list. Allocation stays idle.
        if (veil && !mVeilBuffer.isComplete())
            mVeilBuffer.allocate(1920, 1080, GL_R32F, false, false, LLTexUnit::TT_TEXTURE,
                                 LLTexUnit::TMG_NONE, LLRenderTarget::DEPTH_FMT_32F);
        mReady = (!settings.snowEnabled || mSnowReady) && (!veil || (settings.advancedShelter ? mTiltedVeilReady : mVeilReady));
        mReady = mReady && (!veil || (mVeilBuffer.isComplete() && mVeilComposite.isComplete()));
        if (!mReady)
        {
            mVeilBuffer.release();
            mStatus = "Unavailable: snowfall shader link failed (existing rain available)";
            return false;
        }
        if (!mVAO) glGenVertexArrays(1, &mVAO);
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

void ALWeatherParticles::prepare()
{
#if !LL_DARWIN
    const auto& weather = ALWeatherBridge::Controller::instance();
    if (!weather.precipitationActive()) { suspend(); return; }
    if (!supported())
    {
        mStatus = "Unavailable: requires OpenGL 4.5 compute and texture barriers";
        return;
    }
    const auto& s = weather.settings();
    if (!s.snowEnabled && (!s.visibility || s.visibilityAmount <= 0.f)) { suspend(); return; }
    const U32 cap = s.snowEnabled ? s.snowLimit : mCapacity;
    if (mReady && (!s.snowEnabled || mSnowReady) &&
        (!s.visibility || s.visibilityAmount <= 0.f ||
         ((s.advancedShelter ? mTiltedVeilReady : mVeilReady) && mVeilBuffer.isComplete())) && cap == mCapacity) return;
    if (mAttempted && !mReady) return; // A failed session retries on GL teardown.
    ALWeatherResources::State saved_state;
    LLGLSLShader* previous = LLGLSLShader::sCurBoundShaderPtr;
    const U32 mask = LLVertexBuffer::sLastMask;
    const U32 array_cache = LLVertexBuffer::sGLRenderBuffer, element_cache = LLVertexBuffer::sGLRenderIndices;
    GLint storage = 0, vao = 0, array = 0, element = 0;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &storage);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element);
    if (!mVAO) glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);
    LLVertexBuffer::sLastMask = ~U32(0);
    LLVertexBuffer::setupClientArrays(0);
    if (initialize(cap)) mStatus = "Snow ready (prepared; world cells; bounded GPU list)";
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, storage);
    glBindVertexArray(vao);
    LLVertexBuffer::sLastMask = ~mask;
    LLVertexBuffer::setupClientArrays(mask);
    if (previous) previous->bind(); else LLGLSLShader::unbind();
    glBindBuffer(GL_ARRAY_BUFFER, array); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element);
    LLVertexBuffer::sGLRenderBuffer = array_cache; LLVertexBuffer::sGLRenderIndices = element_cache;
#endif
}

void ALWeatherParticles::render(LLRenderTarget& target, LLRenderTarget& depth,
    const glm::mat4& view, const glm::mat4& projection, const glm::dvec3& origin,
    const glm::vec3& light, ALWeatherResources& cover, U32 frame)
{
    const auto& weather = ALWeatherBridge::Controller::instance();
    if (!weather.precipitationActive())
    {
        mVeilActive = false;
        return;
    }
#if !LL_DARWIN
    if (!supported())
    {
        mVeilActive = false;
        mStatus = "Unavailable: requires OpenGL 4.5 compute and texture barriers";
        return;
    }
    const auto& s = weather.settings();
    mVeilActive = false;
    if (!mReady || (s.snowEnabled && (!mSnowReady || mCapacity != s.snowLimit)) ||
        (s.visibility && s.visibilityAmount > 0.f && !(s.advancedShelter ? mTiltedVeilReady : mVeilReady))) return;
    // Construct hashed names only on first enabled use; steady draws and the
    // off path do not allocate temporary uniform-name strings.
    static const LLStaticHashedString sInverseProjection("snow_inverse_projection"), sLight("snow_light"),
        sVeilLight("snow_veil_light"), sExtinction("snow_extinction"), sInverseView("snow_inverse_view"),
        sCount("snow_count"), sDimension("snow_dimension"), sSlots("snow_slots"), sSeed("snow_seed"),
        sCellOrigin("snow_cell_origin"), sCellRelative("snow_cell_relative"), sAmount("snow_amount"),
        sTime("snow_time"), sView("snow_view"), sProjection("snow_projection"), sScreen("snow_screen"),
        sSize("snow_size"), sRange("snow_range");
    // Preserve bindings not owned by the shared raster state transaction.
    // Advanced samplers append after depth + the retained vertical cover map.
    // Sampler channels are assigned for both runtime branches at link time.
    // The basic hierarchy can therefore occupy a slot after the tilted maps.
    const glm::mat4 inverse_view = glm::inverse(view), inverse_projection = glm::inverse(projection);
    const glm::dvec3 eye = origin + glm::dvec3(inverse_view[3]);
    const bool cover_reachable = cover.precipitationWithin(glm::vec3(inverse_view[3]), origin, frame, 3000.f);
    const U32 cover_units = cover_reachable ? 7U : 0U;
    ALWeatherResources::State saved_state(cover_units, cover_reachable ? 1U : 0U, false);
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
    glBindVertexArray(mVAO);
    LLVertexBuffer::sLastMask = ~U32(0);
    LLVertexBuffer::setupClientArrays(0);
    mVeilActive = s.visibility && s.visibilityAmount > 0.f;
    const F32 extinction = mVeilActive ? SLWeather::precipExtinction(
        weather.state().rainFall, weather.state().snowFall, 0.f) * s.visibilityAmount : 0.f;
    mStatus = cover.precipitationCovers(glm::vec3(inverse_view[3]), origin, frame) ?
        "Snow ready (world cells; bounded GPU list)" :
        "Snow ready; insufficient cover for this view (prepare its recording area)";
    std::array<GLenum, 4> draw_buffers{};
    const U32 buffer_count = cover_reachable ? std::min(target.getNumTextures(), U32(draw_buffers.size())) : 0U;
    const GLenum beauty = GL_COLOR_ATTACHMENT0;
    // OpenGL 4.5 / ARB_texture_barrier, section 9.3.1: prior depth writes
    // become visible to texture fetches. Both draws keep depth writes disabled;
    // neither shader samples a colour attachment it writes. No feedback writes.
    if (cover_reachable)
    {
        target.bindTarget();
        for (U32 i = 0; i < buffer_count; ++i)
        {
            GLint buffer = 0;
            glGetIntegerv(GL_DRAW_BUFFER0 + i, &buffer);
            draw_buffers[i] = GLenum(buffer);
        }
        glDrawBuffers(1, &beauty);
        glTextureBarrier();
        glEnablei(GL_BLEND, 0);
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        glBlendFuncSeparatei(0, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    LLGLDisable cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
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
    if (extinction > 0.f && cover_reachable)
    {
        LL_PROFILE_GPU_ZONE("weather veil consumer");
        LLGLDepthTest no_depth(GL_FALSE, GL_FALSE);
        mVeilBuffer.bindTarget();
        glDisablei(GL_BLEND, 0);
        const U32 field_width = std::min((target.getWidth()+1)/2, mVeilBuffer.getWidth());
        const U32 field_height = std::min((target.getHeight()+1)/2, mVeilBuffer.getHeight());
        glViewport(0, 0, field_width, field_height);
        LLGLSLShader& veil = s.advancedShelter ? mTiltedVeil : mVeil;
        bind(veil);
        veil.uniformMatrix4fv(sInverseView, 1, false, glm::value_ptr(inverse_view));
        glBindVertexArray(mVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3); // Scalar integration, no beauty extinction yet.
        LLGLSLShader::unbind();
        mVeilBuffer.flush();
        // flush() restores the parent target and viewport. Rebinding the same
        // parent would insert it twice into LLRenderTarget's logical stack.
        glDrawBuffers(1, &beauty);
        glEnablei(GL_BLEND, 0);
        mVeilComposite.bind();
        mVeilComposite.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &depth, true, LLTexUnit::TFO_POINT);
        mVeilComposite.bindTexture(LLShaderMgr::WEATHER_VEIL_MAP, &mVeilBuffer, false, LLTexUnit::TFO_POINT);
        static const LLStaticHashedString field_size("snow_veil_field_size");
        mVeilComposite.uniform2f(field_size, F32(field_width), F32(field_height));
        mVeilComposite.uniformMatrix4fv(sInverseProjection, 1, false, glm::value_ptr(inverse_projection));
        mVeilComposite.uniform3f(sVeilLight, light.x*.8f, light.y*.8f, light.z*.8f);
        glDrawArrays(GL_TRIANGLES, 0, 3); // One extinction application per target view.
        LLGLSLShader::unbind();
    }
    if (s.snowEnabled && ALWeatherBridge::snowAmount(weather.state()) > 0.f)
    {
        LL_PROFILE_GPU_ZONE("weather snow consumer");
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
        if (cover_reachable)
        {
            LLGLDepthTest depth_test(GL_TRUE, GL_FALSE, GL_LEQUAL);
            bind(mDraw);
            mDraw.uniformMatrix4fv(sView, 1, false, glm::value_ptr(view));
            mDraw.uniformMatrix4fv(sProjection, 1, false, glm::value_ptr(projection));
            mDraw.uniform2f(sScreen, F32(target.getWidth()), F32(target.getHeight()));
            mDraw.uniform1f(sSize, s.flakeSize);
            mDraw.uniform1f(sRange, s.snowRange);
            glBindVertexArray(mVAO); glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
            const GLuint position = mPosition;
            const GLuint traits = mTraits;
            glEnableVertexAttribArray(position); glEnableVertexAttribArray(traits);
            glVertexAttribPointer(position, 4, GL_FLOAT, GL_FALSE, 32, nullptr);
            glVertexAttribPointer(traits, 4, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(16));
            glVertexAttribDivisor(position, 1); glVertexAttribDivisor(traits, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
        }
    }
    if (cover_reachable)
    {
        glDrawBuffers(buffer_count, draw_buffers.data());
        target.flush();
    }
    restore();
#else
    mStatus = "Unavailable: requires OpenGL 4.5 compute and texture barriers";
#endif
}
