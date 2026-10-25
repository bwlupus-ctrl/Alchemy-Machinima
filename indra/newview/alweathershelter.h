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
#include <algorithm>

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
    bool auxiliaryAllowed(bool library, bool active, bool advanced);
    // Affine precipitation coordinates: (x + shear.x*z, y + shear.y*z, z).
    // Height remains vertical; a slanted roof sample is never a ground height.
    struct Plan
    {
        std::array<F64, 3> center{};
        std::array<F32, 2> shear{};
        F32 extent = 0.f, vertical = 0.f;
        U32 resolution = 0;
        bool operator==(const Plan& p) const;
    };
    Plan plan(const std::array<F64, 3>& eye, F32 extent, U32 resolution,
              const std::array<F32, 2>& shear);
    std::array<F32, 2> fallShear(F32 wind_x, F32 wind_y, F32 speed, bool follow);
    class Updates
    {
    public:
        struct Tile
        {
            U32 x = 0, y = 0, size = 8;
            bool full() const { return size == 8; }
        };
        // Bounded, spatial dirty work shared by basic cover and tilted shelter.
        // Each call selects one tile, including initial/replacement maps.
        void dirty(const std::array<F64, 3>& low, const std::array<F64, 3>& high);
        bool schedule(const Plan& close, const Plan& distant, U32 place,
                      const std::array<F64, 3>& origin, F64 seconds, U32 hz, U32 layers = 2);
        const Plan& selectedPlan() const { return mPlans[mSelected]; }
        U32 selectedLayer() const { return mSelected; }
        Tile tile() const { return mTile; }
        void complete();
        bool layerComplete() const { return mComplete[mSelected]; }
        U32 pendingTiles() const;
        void noteWork(U64 chunks) { mWork[mSelected][mTile.y*8 + mTile.x] = chunks; }
        F64 stalenessSeconds() const;
        F64 oldestSeconds() const;
        bool initialized() const { return mInitialized; }
        void publish() { mValid = true; }
        void invalidate() { mValid = false; }
    private:
        std::array<Plan, 2> mPlans{};
        std::array<U64, 2> mDirty{}, mFollowup{}, mBuilt{};
        std::array<bool, 2> mComplete{};
        U32 mSelected = 0, mCursor = 0, mLayerCount = 2;
        std::array<U32, 2> mTileCursor{};
        Tile mTile;
        bool mInitialized = false;
        std::array<F64, 3> mOrigin{};
        U32 mPlace = 0;
        F64 mNext = 0., mLast = -1., mSlotSeconds = 0.05;
        std::array<std::array<U64, 64>, 2> mWork{};
        std::array<std::array<F64, 64>, 2> mSince{};
        bool mValid = false;
    };
    constexpr U64 coverBytes(U32 resolution) { return U64(resolution) * resolution * 4 * 2; }
    constexpr U64 intervalBytes(U32 resolution) { return resolution ? (U64(resolution)*resolution-1)*8/3 : 0; }
    constexpr U64 advancedBytes(U32 close_resolution, U32 distant_resolution)
    {
        // Four FBOs plus the dedicated program/driver bookkeeping reserve.
        return coverBytes(close_resolution) + coverBytes(distant_resolution) +
            intervalBytes(close_resolution) + intervalBytes(distant_resolution) +
            (close_resolution || distant_resolution ? 256ULL * 1024 : 0);
    }
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
