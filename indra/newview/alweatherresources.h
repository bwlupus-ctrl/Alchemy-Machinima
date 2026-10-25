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
#include "llglslshader.h"
#include <glm/glm.hpp>
#include <string>
#include <array>
class ALWeatherShelterMaps;
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
        GLint mProgramName;
        GLuint mCachedProgram;
        LLGLSLShader* mProgram;
        U32 mActive, mUnits, mBuffers;
        bool mFull;
    };
    bool allocate(U32 resolution);
    void prepare(U32 resolution);
    void prepareIntervals();
    void buildIntervals(ALWeatherShelter::Updates::Tile tile);
    void bindIntervals(LLGLSLShader* shader, S32 sampler);
    bool copyFrom(ALWeatherResources& source);
    void setBounds(const ALWeatherShelter::Plan& plan, const glm::dvec3& origin);
    bool matches(const ALWeatherShelter::Plan& plan, const glm::dvec3& origin) const
    { return mBoundsValid && plan == mPlan && origin == mOrigin; }
    bool precipitationWithin(const glm::vec3& eye, const glm::dvec3& origin, U32 frame, F32 reach) const;
    void release();
    // A tile patches the completed map between draws. Full replacements still
    // use the other FBO and publish texture + transform atomically.
    void patch(bool enabled) { mPatching = enabled && ready(); }
    LLRenderTarget& writeTarget() { return mCover[mPatching ? mRead : 1 - mRead]; }
    void publish(const glm::mat4& matrix, F32 range, U32 batches, bool complete,
                 const glm::dvec3& origin, U32 place);
    void fail(ALWeatherShelter::Coverage state, const char* reason = nullptr);
    void bind(LLGLSLShader* shader, const glm::mat4& view_to_agent, const glm::dvec3& origin, U32 frame);
    void bindPrecipitation(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame);
    void bindMap(LLGLSLShader* shader, S32 sampler, const LLStaticHashedString& matrix);
    ALWeatherShelterMaps* advanced = nullptr;
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
    friend class ALWeatherShelterMaps;
    LLRenderTarget mCover[2];
    U32 mRead = 0, mPlace = 0;
    bool mPatching = false;
    LLGLSLShader mReduction;
    U32 mMinimum = 0, mMinimumResolution = 0;
    bool mMinimumValid = false, mMinimumAttempted = false;
    U64 mContent = 0, mSourceContent = 0;
    ALWeatherResources* mSource = nullptr;
    ALWeatherShelter::Plan mPlan;
    std::array<F32, 3> mLow{}, mHigh{};
    bool mBoundsValid = false;
    U32 mFailedResolution = 0;
    U32 mPublishedFrame = ~U32(0);
    glm::mat4 mMatrix{1.f};
    glm::dvec3 mOrigin{0.};
    F32 mRange = 1.f;
    ALWeatherShelter::Coverage mCoverage = ALWeatherShelter::Coverage::INCOMPLETE;
    std::string mStatus = "Persistent wet surfaces off";
};

// Dedicated targets; never borrows a sun, spot, reflection or probe shadow map.
// The basic owner above remains the untilted height source.
class ALWeatherShelterMaps
{
public:
    void prepare();
    void release();
    void suspend()
    {
        mPublishedFrame = preparedFrame = ~U32(0);
        updates.invalidate();
        mStatus = "Shelter off";
    }
    bool prepared() const { return mReady; }
    bool current(const glm::dvec3& origin, U32 frame) const;
    bool covers(const glm::vec3& eye, const glm::dvec3& origin, U32 frame) const;
    bool within(const glm::vec3& eye, const glm::dvec3& origin, U32 frame, F32 reach) const;
    void bind(LLGLSLShader* shader, const glm::dvec3& origin, U32 frame);
    LLGLSLShader& rainProgram() { return mRain; }
    const std::string& status() const { return mStatus; }
    void fail(const char* reason);
    void publish(U32 frame);
    void reuse(U32 frame)
    {
        if (mPublishedFrame == ~U32(0)) return;
        for (auto& layer : layers) layer.reuseForFrame(frame);
        mPublishedFrame = frame;
    }
    ALWeatherResources layers[2];
    ALWeatherShelter::Updates updates;
    U32 preparedFrame = ~U32(0);
private:
    LLGLSLShader mRain;
    U32 mResolution[2]{};
    U32 mAttemptGeneration = ~U32(0), mPublishedFrame = ~U32(0);
    U32 mPlace = 0;
    glm::dvec3 mOrigin{0.};
    bool mReady = false;
    std::string mStatus = "Shelter off";
};
#endif
