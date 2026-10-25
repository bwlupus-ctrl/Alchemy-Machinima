/**
 * @file alweatherparticles.h
 * @brief Bounded native GL snowfall; view lists never advance weather.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream Viewer precipitation design reference.
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_PARTICLES_H
#define AL_WEATHER_PARTICLES_H
#include "llglslshader.h"
#include <glm/glm.hpp>
class LLRenderTarget;
class ALWeatherResources;

class ALWeatherParticles
{
public:
    static bool supported();
    void release();
    bool allocated() const { return mVAO || mBuffer || mAttempted; }
    bool veilActive() const { return mVeilActive; }
    bool available() const { return !mAttempted || mReady; }
    const char* status() const { return mStatus; }
    U32 bytes() const { return mCapacity * 32U; }
    void render(LLRenderTarget& target, LLRenderTarget& depth,
                const glm::mat4& view, const glm::mat4& projection,
                const glm::dvec3& origin, const glm::vec3& light,
                ALWeatherResources& cover, U32 frame);
private:
    bool initialize(U32 cap);
    LLGLSLShader mSimulation, mDraw, mVeil;
    GLuint mBuffer = 0, mVAO = 0;
    U32 mCapacity = 0;
    bool mAttempted = false, mReady = false, mVeilActive = false;
    const char* mStatus = "Falling snow off";
};
#endif
