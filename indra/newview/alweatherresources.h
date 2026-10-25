/**
 * @file alweatherresources.h
 * @brief Shared, double-buffered GL weather cover ownership. No view advances simulation.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_RESOURCES_H
#define AL_WEATHER_RESOURCES_H
#include "alweathershelter.h"
#include "llrendertarget.h"
#include <glm/glm.hpp>
#include <string>
#include <array>
class LLGLSLShader;
class ALWeatherResources
{
public:
    // Covers allocation and every early return as well as the complete shadow traversal.
    class State
    {
    public:
        State(U32 units = 32, U32 buffers = 32, bool full = true);
        ~State();
    private:
        struct Texture { LLTexUnit::eTextureType type; U32 id; bool mips; };
        struct DrawState { GLint buffer, src, dst, srcAlpha, dstAlpha, eq, eqAlpha; GLboolean mask[4], blend; };
        std::array<Texture, 32> mTextures{};
        std::array<DrawState, 32> mDraw{};
        LLRender::eBlendFactor mCS, mCD, mAS, mAD;
        GLint mDrawFBO, mReadFBO, mReadBuffer, mCull, mFront, mViewport[4], mDepthFunc;
        GLboolean mDepthMask;
        GLdouble mClearDepth;
        U32 mActive, mUnits, mBuffers;
        bool mFull;
    };
    bool allocate(U32 resolution);
    void release();
    LLRenderTarget& writeTarget() { return mCover[1 - mRead]; }
    void publish(const glm::mat4& matrix, F32 range, U32 batches, bool complete,
                 const glm::dvec3& origin, U32 place);
    void fail(ALWeatherShelter::Coverage state, const char* reason = nullptr);
    void bind(LLGLSLShader* shader, const glm::mat4& view_to_agent, const glm::dvec3& origin, U32 frame);
    void bindPrecipitation(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame);
    bool precipitationCovers(const glm::vec3& eye, const glm::dvec3& origin, U32 frame) const;
    bool ready() const { return ALWeatherShelter::usable(mCoverage); }
    bool readyForFrame(U32 frame) const { return ready() && mPublishedFrame == frame; }
    void reuseForFrame(U32 frame) { mPublishedFrame = frame; }
    bool allocated() const { return mCover[0].getWidth() != 0 || mCover[1].getWidth() != 0; }
    bool hasState() const { return allocated() || mCoverage != ALWeatherShelter::Coverage::INCOMPLETE; }
    bool allocationFailed(U32 resolution) const { return mFailedResolution == resolution; }
    const std::string& status() const { return mStatus; }
    U32 mPreparedFrame = ~U32(0);
private:
    LLRenderTarget mCover[2];
    U32 mRead = 0, mPlace = 0;
    U32 mFailedResolution = 0;
    U32 mPublishedFrame = ~U32(0);
    glm::mat4 mMatrix{1.f};
    glm::dvec3 mOrigin{0.};
    F32 mRange = 1.f;
    ALWeatherShelter::Coverage mCoverage = ALWeatherShelter::Coverage::INCOMPLETE;
    std::string mStatus = "Persistent wet surfaces off";
};
#endif
