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
    LL_PROFILE_GPU_ZONE("weather GL state save");
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
    LL_PROFILE_GPU_ZONE("weather GL state restore");
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
        glBlendFuncSeparate(mDraw[0].src, mDraw[0].dst, mDraw[0].srcAlpha, mDraw[0].dstAlpha);
        gGL.setColorMask(mDraw[0].mask[0], mDraw[0].mask[1], mDraw[0].mask[2], mDraw[0].mask[3]);
        if (mDraw[0].blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glBlendEquationSeparate(mDraw[0].eq, mDraw[0].eqAlpha);
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
        // Restore the common global state once, then only indexed exceptions.
        // Partial consumer guards must keep all other attachments untouched.
        if (!mFull || std::memcmp(d.mask, mDraw[0].mask, sizeof(d.mask)))
            glColorMaski(i, d.mask[0], d.mask[1], d.mask[2], d.mask[3]);
        if (!mFull || d.blend != mDraw[0].blend)
        {
            if (d.blend) glEnablei(GL_BLEND, i); else glDisablei(GL_BLEND, i);
        }
        if (!mFull || d.src != mDraw[0].src || d.dst != mDraw[0].dst ||
            d.srcAlpha != mDraw[0].srcAlpha || d.dstAlpha != mDraw[0].dstAlpha)
            glBlendFuncSeparatei(i, d.src, d.dst, d.srcAlpha, d.dstAlpha);
        if (mFull && (d.eq != mDraw[0].eq || d.eqAlpha != mDraw[0].eqAlpha))
            glBlendEquationSeparatei(i, d.eq, d.eqAlpha);
    }
    if (mProgramName == 0 || glIsProgram(mProgramName))
    {
        glUseProgram(mProgramName);
        LLGLSLShader::sCurBoundShader = mCachedProgram;
        LLGLSLShader::sCurBoundShaderPtr = mProgram;
    }
    else LLGLSLShader::unbind();
}

void ALWeatherResources::preloadShaders()
{
#if !LL_DARWIN
    if (!LLShaderMgr::computeShadersSupported()) return;
    if (!mReduction.isComplete())
    {
        mReduction.mName = "Weather cover interval hierarchy";
        mReduction.mShaderLevel = 1;
        mReduction.mFeatures = LLShaderFeatures();
        mReduction.mShaderFiles.clear();
        mReduction.mShaderFiles.emplace_back("deferred/weatherCoverMinimum.glsl", GL_COMPUTE_SHADER);
        if (!mReduction.createShader()) return; // Exact leaf walk remains available.
    }
#endif
}

void ALWeatherResources::prepare(U32 resolution)
{
    if (allocationFailed(resolution)) return;
    if (mCover[0].getWidth() == resolution && mCover[1].getWidth() == resolution && mMinimumAttempted) return;
    State saved_state;
    if (allocate(resolution)) prepareIntervals();
}
void ALWeatherResources::prepareIntervals()
{
#if !LL_DARWIN
    mMinimumAttempted = true;
    const U32 resolution = mCover[0].getWidth();
    if (!resolution || !LLShaderMgr::computeShadersSupported()) return;
    if (!mReduction.isComplete()) return; // Startup failure uses the exact leaf walk.
    if (mMinimum && mMinimumResolution == resolution) return;
    if (mMinimum) glDeleteTextures(1, &mMinimum);
    glGenTextures(1, &mMinimum);
    gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mMinimum, false);
    U32 levels = 0;
    for (U32 size = resolution/2; size; size /= 2, ++levels)
        glTexImage2D(GL_TEXTURE_2D, levels, GL_RG32F, size, size, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels-1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    mMinimumResolution = resolution;
    mMinimumValid = false;
#endif
}
void ALWeatherResources::beginTile(const ALWeatherShelter::Plan& plan)
{
    if (!mStaging || !(plan == mStagingPlan))
    {
        mStagingPlan = plan; mStaging = true; mRefreshCursor = 0;
    }
    mPatching = false;
    mRefreshComplete = false;
    if (!ready()) mCoverage = ALWeatherShelter::Coverage::INCOMPLETE;
}
void ALWeatherResources::commitTile(ALWeatherShelter::Updates::Tile tile, bool replacement_complete)
{
    mRefreshCursor = 0;
    // A changed transform is published only after every replacement tile is
    // complete. Until then the old texture and transform remain paired.
    if (!matches(mStagingPlan, mOrigin) || !ready())
    {
        // Amortize the hierarchy alongside replacement leaves. Until every
        // leaf is built, old-depth consumers use the exact sampling fallback;
        // a hierarchy mixing two transforms is never advertised as valid.
        buildIntervals(tile);
        if (!replacement_complete) mMinimumValid = false;
        return;
    }
    const U32 size = writeTarget().getWidth(), cell = size/8;
    const U32 x = tile.x*cell, y = tile.y*cell, extent = tile.size*cell;
    LLGLDisable no_scissor(GL_SCISSOR_TEST);
    mCover[mRead].copyContents(writeTarget(), x, y, x+extent, y+extent,
        x, y, x+extent, y+extent, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    mPatching = true;
    buildIntervals(tile);
}
void ALWeatherResources::refreshStatus(const ALWeatherShelter::Updates& updates)
{
    // Replace rather than append on reused frames.
    const auto suffix = mStatus.find("; refresh ");
    if (suffix != std::string::npos) mStatus.erase(suffix);
    mStatus += "; refresh " + std::to_string(updates.pendingTiles()) +
        " tiles pending; oldest " + std::to_string(updates.oldestSeconds()) +
        " s; 1 tile/frame, 16 draws/2048 triangles/frame; queue bound " +
        std::to_string(updates.stalenessSeconds()) + " s at 60 Hz for observed caster load (including follow-up)";
}
void ALWeatherResources::buildIntervals(ALWeatherShelter::Updates::Tile tile)
{
#if !LL_DARWIN
    if (!mMinimum || !mReduction.isComplete()) { mMinimumValid = false; return; }
    LL_PROFILE_GPU_ZONE("weather interval dirty updates");
    GLint saved[4][6];
    const GLenum names[] = {GL_IMAGE_BINDING_NAME, GL_IMAGE_BINDING_LEVEL, GL_IMAGE_BINDING_LAYERED,
        GL_IMAGE_BINDING_LAYER, GL_IMAGE_BINDING_ACCESS, GL_IMAGE_BINDING_FORMAT};
    for (U32 unit = 0; unit < 4; ++unit)
        for (U32 i = 0; i < 6; ++i) glGetIntegeri_v(names[i], unit, &saved[unit][i]);
    mReduction.bind();
    static const LLStaticHashedString input("cover_input"), level("cover_level"), rectangle("cover_rectangle"), depth_input("cover_depth_input"), level_count("cover_levels");
    mReduction.uniform1i(input, 0);
    const U32 resolution = writeTarget().getWidth();
    U32 x = tile.x*resolution/8, y = tile.y*resolution/8;
    U32 end_x = x + tile.size*resolution/8, end_y = y + tile.size*resolution/8;
    // One 8x8 workgroup produces four successive levels using shared memory.
    // Expand to workgroup boundaries so parents include unchanged neighbour
    // leaves as well as this tile; incomplete replacements remain unpublished.
    U32 total_levels = 0;
    for (U32 size = resolution/2; size; size /= 2) ++total_levels;
    for (U32 mip = 0; mip < total_levels;)
    {
        x /= 2; y /= 2; end_x = (end_x+1)/2; end_y = (end_y+1)/2;
        const U32 size = resolution >> (mip+1);
        x = x/8*8; y = y/8*8;
        end_x = std::min(size, (end_x+7)/8*8);
        end_y = std::min(size, (end_y+7)/8*8);
        const U32 levels = std::min(4U, total_levels-mip);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mip ? mMinimum : writeTarget().getDepth(), false);
        mReduction.uniform1i(level, mip ? mip-1 : 0);
        mReduction.uniform1i(depth_input, mip ? 0 : 1);
        mReduction.uniform4f(rectangle, F32(x), F32(y), F32(end_x-x), F32(end_y-y));
        mReduction.uniform1i(level_count, levels);
        for (U32 unit = 0; unit < levels; ++unit)
            glBindImageTexture(unit, mMinimum, mip+unit, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
        glDispatchCompute((end_x-x+7)/8, (end_y-y+7)/8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        // The next dispatch reads the last level produced by this group.
        for (U32 i = 1; i < levels; ++i)
        {
            x /= 2; y /= 2; end_x = (end_x+1)/2; end_y = (end_y+1)/2;
        }
        mip += levels;
    }
    for (U32 unit = 0; unit < 4; ++unit)
        glBindImageTexture(unit, saved[unit][0], saved[unit][1], GLboolean(saved[unit][2]),
            saved[unit][3], saved[unit][4], saved[unit][5]);
    mMinimumValid = glGetError() == GL_NO_ERROR;
#endif
}
void ALWeatherResources::bindIntervals(LLGLSLShader* shader, S32 sampler)
{
    const S32 unit = shader->getTextureChannel(sampler);
    if (unit >= 0) gGL.getTexUnit(unit)->bindManual(LLTexUnit::TT_TEXTURE, mMinimumValid ? mMinimum : 0, false);
}
bool ALWeatherResources::copyFrom(ALWeatherResources& source)
{
    if (!source.readyForFrame(mPreparedFrame) || source.mCover[source.mRead].getWidth() != mCover[0].getWidth()) return false;
    if (ready() && mSource == &source && mSourceContent == source.mContent)
    {
        reuseForFrame(mPreparedFrame);
        return true;
    }
    LL_PROFILE_GPU_ZONE("weather reuse vertical map");
    LLGLDisable no_scissor(GL_SCISSOR_TEST);
    patch(false);
    const U32 size = source.mCover[source.mRead].getWidth();
    writeTarget().copyContents(source.mCover[source.mRead], 0, 0, size, size, 0, 0, size, size,
                              GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    if (glGetError() != GL_NO_ERROR) return false;
    buildIntervals({});
    mPlan = source.mPlan; mLow = source.mLow; mHigh = source.mHigh; mBoundsValid = source.mBoundsValid;
    publish(source.mMatrix, source.mRange, source.mCoverage == ALWeatherShelter::Coverage::COMPLETE_COVERED ? 1 : 0,
        true, source.mOrigin, source.mPlace);
    if (!ready()) return false;
    mSource = &source; mSourceContent = source.mContent;
    return true;
}
void ALWeatherResources::setBounds(const ALWeatherShelter::Plan& plan, const glm::dvec3& origin)
{
    mPlan = plan;
    const F64 bottom = plan.center[2] - plan.vertical, top = plan.center[2] + plan.vertical;
    const F64 offset[] = {origin.x, origin.y, origin.z};
    for (U32 axis = 0; axis < 2; ++axis)
    {
        const F64 a = plan.shear[axis]*bottom, b = plan.shear[axis]*top;
        mLow[axis] = F32(plan.center[axis]-plan.extent-std::max(a,b)-offset[axis]-1.);
        mHigh[axis] = F32(plan.center[axis]+plan.extent-std::min(a,b)-offset[axis]+1.);
    }
    mLow[2] = F32(bottom-origin.z-1.); mHigh[2] = F32(top-origin.z+1.);
    mBoundsValid = true;
}
bool ALWeatherResources::precipitationWithin(const glm::vec3& eye, const glm::dvec3& origin, U32 frame, F32 reach) const
{
    if (ALWeatherBridge::Controller::instance().settings().advancedShelter && advanced)
        return advanced->within(eye, origin, frame, reach);
    if (!readyForFrame(frame) || origin != mOrigin || ALWeatherBridge::Controller::instance().placeSerial() != mPlace) return false;
    if (!mBoundsValid) return true; // No bound proof: keep the exact shader path.
    const F32 position[] = {eye.x, eye.y, eye.z};
    F64 distance_squared = 0.;
    for (U32 axis = 0; axis < 3; ++axis)
    {
        const F64 delta = std::max({F64(mLow[axis]-position[axis]), 0., F64(position[axis]-mHigh[axis])});
        distance_squared += delta*delta;
    }
    return distance_squared <= F64(reach)*reach;
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
    if (mMinimum) glDeleteTextures(1, &mMinimum);
    mMinimum = mMinimumResolution = 0; mMinimumValid = mMinimumAttempted = false;
    // Keep the resident reduction program across off/resize and map reallocation.
    mPatching = false; mStaging = false; mRefreshCursor = 0;
    mSource = nullptr; ++mContent;
    mBoundsValid = false;
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
    LL_PROFILE_GPU_ZONE("weather cover publication");
    const F32* values = glm::value_ptr(matrix);
    const bool finite_map = std::all_of(values, values + 16, [](F32 value) { return std::isfinite(value); }) &&
        std::isfinite(range) && range > 0.f && std::isfinite(origin.x) &&
        std::isfinite(origin.y) && std::isfinite(origin.z);
    if (!finite_map)
    {
        fail(ALWeatherShelter::Coverage::FAILED, "Surface cover publication invalid; coverage unknown");
        return;
    }
    // An empty patch does not prove the previously completed whole map empty.
    const U32 known_batches = mPatching && mCoverage == ALWeatherShelter::Coverage::COMPLETE_COVERED ?
        std::max(batches, 1U) : batches;
    fail(ALWeatherShelter::classify(writeTarget().isComplete(), true, true, complete, known_batches));
    if (!ready()) return;
    mMatrix = matrix;
    mRange = range;
    mOrigin = origin;
    mPlace = place;
    mStatus += " (" + std::to_string(writeTarget().getWidth()) + " px; +/-" +
        std::to_string(U32(ALWeatherBridge::Controller::instance().settings().coverExtent)) + " m; " +
        std::to_string(ALWeatherShelter::coverBytes(writeTarget().getWidth()) / 1024) + " KiB)";
    if (!mPatching) mRead = 1 - mRead; // Texture and all stamps publish together after flush.
    mPatching = false;
    mSource = nullptr; ++mContent;
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
    static const LLStaticHashedString minimum_valid("weather_cover_minimum_valid");
    shader->uniform1i(minimum_valid, mMinimumValid ? 1 : 0);
    static const LLStaticHashedString sCoverValid("snow_cover_valid");
    const bool current = readyForFrame(frame) && origin == mOrigin &&
        ALWeatherBridge::Controller::instance().placeSerial() == mPlace;
    const bool tilted = ALWeatherBridge::Controller::instance().settings().advancedShelter && advanced;
    shader->uniform1i(sCoverValid, tilted ? (advanced->current(origin, frame) ? 2 : 0) : (current ? 1 : 0));
    if (tilted) advanced->bind(shader, origin, frame);
    if (!current) return;
    shader->bindTexture(LLShaderMgr::WEATHER_SURFACE_COVER_MAP, &mCover[mRead], true, LLTexUnit::TFO_POINT);
    bindIntervals(shader, LLShaderMgr::WEATHER_SURFACE_COVER_MIN_MAP);
    shader->uniformMatrix4fv(LLShaderMgr::WEATHER_SURFACE_COVER_MATRIX, 1, false, glm::value_ptr(mMatrix));
}

void ALWeatherResources::bindMap(LLGLSLShader* shader, S32 sampler, const LLStaticHashedString& matrix)
{
    shader->bindTexture(sampler, &mCover[mRead], true, LLTexUnit::TFO_POINT);
    shader->uniformMatrix4fv(matrix, 1, false, glm::value_ptr(mMatrix));
}

void ALWeatherShelterMaps::preloadShaders()
{
    bool barriers = false;
#if !LL_DARWIN
    barriers = glMemoryBarrier != nullptr;
#endif
    if (!LLRender::indexedDrawBufferGuardSupported() || gGLManager.mNumTextureImageUnits < 4 ||
        gGLManager.mGLVersion < 4.19f || !barriers) return;
    for (auto& layer : layers) layer.preloadShaders();
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
    for (auto& layer : layers) layer.prepareIntervals();
    if (!mRain.isComplete())
    {
        fail("Advanced shelter shader failed at startup; coverage unknown, existing fallback");
        return;
    }
    mReady = true;
    mStatus = "Advanced shelter preparing; coverage unknown";
}
void ALWeatherShelterMaps::release()
{
    for (auto& layer : layers) layer.release();
    // The central shader inventory owns program teardown.
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
bool ALWeatherShelterMaps::within(const glm::vec3& eye, const glm::dvec3& origin, U32 frame, F32 reach) const
{
    return current(origin, frame) && (layers[0].precipitationWithin(eye, origin, frame, reach) ||
                                     layers[1].precipitationWithin(eye, origin, frame, reach));
}
void ALWeatherShelterMaps::publish(U32 frame)
{
    const auto& weather = ALWeatherBridge::Controller::instance();
    if (!mReady || !layers[0].readyForFrame(frame) || !layers[1].readyForFrame(frame) ||
        layers[0].mOrigin != layers[1].mOrigin || layers[0].mPlace != layers[1].mPlace ||
        layers[0].mPlace != weather.placeSerial())
    {
        mStatus = "Advanced shelter refreshing; previous valid maps retained; " +
            std::to_string(updates.pendingTiles()) + " tiles pending";
        return;
    }
    mOrigin = layers[0].mOrigin;
    mPlace = weather.placeSerial();
    mPublishedFrame = frame;
    updates.publish();
    const auto& s = weather.settings();
    mStatus = "Advanced shelter ready; near +/-" + std::to_string(U32(s.shelterNearExtent)) +
        " m, far +/-" + std::to_string(U32(s.shelterFarExtent)) + " m; update <=" +
        std::to_string(s.shelterHz) + " Hz; outside coverage unknown; blended glass excluded; " + layers[0].status();
}
void ALWeatherShelterMaps::bind(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame)
{
    static const LLStaticHashedString matrices[2] = {LLStaticHashedString("weather_shelter_close_matrix"), LLStaticHashedString("weather_shelter_distant_matrix")};
    static const LLStaticHashedString params("weather_shelter_params");
    if (!current(origin, frame)) return;
    layers[0].bindMap(shader, LLShaderMgr::WEATHER_SHELTER_CLOSE_MAP, matrices[0]);
    layers[1].bindMap(shader, LLShaderMgr::WEATHER_SHELTER_DISTANT_MAP, matrices[1]);
    layers[0].bindIntervals(shader, LLShaderMgr::WEATHER_SHELTER_CLOSE_MIN_MAP);
    layers[1].bindIntervals(shader, LLShaderMgr::WEATHER_SHELTER_DISTANT_MIN_MAP);
    static const LLStaticHashedString minimum_valid("weather_shelter_minimum_valid");
    shader->uniform1i(minimum_valid, layers[0].mMinimumValid && layers[1].mMinimumValid ? 1 : 0);
    const auto& s = ALWeatherBridge::Controller::instance().settings();
    shader->uniform4f(params, s.shelterBias, s.shelterSoftness, layers[0].mRange, layers[1].mRange);
    static const LLStaticHashedString debug("weather_shelter_debug");
    shader->uniform1i(debug, s.shelterDebug ? 1 : 0);
}
