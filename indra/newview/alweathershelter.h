/**
 * @file alweathershelter.h
 * @brief Basic surface cover publication and bounded weather probe revisions.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_SHELTER_H
#define AL_WEATHER_SHELTER_H
#include "alweatherbridge.h"

namespace ALWeatherShelter
{
    // Each camera owns its own presentation-time fade, independent of draw order.
    class Exposure
    {
    public:
        void reset();
        F32 update(F64 presentation_time, F32 target);
    private:
        F32 mExposure = 1.f;
        F64 mLastPresentationTime = 0.0;
        bool mInitialized = false;
    };
    enum class Coverage : U32 { INCOMPLETE, COMPLETE_EMPTY, COMPLETE_COVERED, FAILED };
    // A completed clear + terrain/caster traversal is valid even without a roof.
    Coverage classify(bool allocated, bool cleared, bool traversed, bool complete, U32 batches);
    bool usable(Coverage state);
    const char* label(Coverage state);
    constexpr U64 coverBytes(U32 resolution) { return U64(resolution) * resolution * 4 * 2; }
    U64 materialSignature(const ALWeatherBridge::Controller& weather);
    class ProbeRevision
    {
    public:
        // Quantized states coalesce at 2 Hz; scheduler face/work caps remain authoritative.
        bool update(U64 signature, F64 seconds);
        U64 signature() const { return mPublished; }
    private:
        U64 mPublished = 0;
        F64 mNext = 0.;
        F64 mLast = -1.;
        bool mValid = false;
    };
}
#endif
