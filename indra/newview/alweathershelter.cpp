/**
 * @file alweathershelter.cpp
 * @brief Surface coverage differs deliberately from legacy rain caster validity.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alweathershelter.h"
#include <algorithm>
#include <cmath>

namespace ALWeatherShelter
{
    bool auxiliaryAllowed(bool library, bool active, bool advanced)
    {
        return library ? active : advanced;
    }
    bool Plan::operator==(const Plan& p) const
    {
        return center == p.center && shear == p.shear && extent == p.extent &&
            vertical == p.vertical && resolution == p.resolution;
    }
    std::array<F32, 2> fallShear(F32 wind_x, F32 wind_y, F32 speed, bool follow)
    {
        if (!follow || !std::isfinite(wind_x) || !std::isfinite(wind_y)) return {};
        const F32 fall_speed = std::isfinite(speed) ? std::max(speed, 1.f) : 28.f;
        // A 45 degree limit also bounds the caster cull volume. No gust input.
        const F32 scale = 1.f / std::max(fall_speed, std::hypot(wind_x, wind_y));
        // Following a falling ray preserves x + wind.x/fall_speed * z.
        return {wind_x * scale, wind_y * scale};
    }
    Plan plan(const std::array<F64, 3>& eye, F32 extent, U32 resolution,
              const std::array<F32, 2>& shear)
    {
        Plan p;
        p.extent = extent; p.resolution = resolution; p.shear = shear;
        p.vertical = std::clamp(extent + 32.f, 64.f, 512.f);
        const F64 cell = 2. * extent / resolution;
        // Snap the WORLD precipitation lattice, not the camera/view coordinates.
        p.center = {std::floor((eye[0] + shear[0]*eye[2])/cell + 0.5)*cell,
                    std::floor((eye[1] + shear[1]*eye[2])/cell + 0.5)*cell,
                    std::floor(eye[2]/8. + 0.5)*8.};
        return p;
    }
    void Updates::dirty(const std::array<F64, 3>& low, const std::array<F64, 3>& high)
    {
        for (U32 layer = 0; layer < 2; ++layer)
        {
            const auto& p = mPlans[layer];
            if (!mInitialized || p.extent <= 0.f || high[2] < p.center[2] - p.vertical ||
                low[2] > p.center[2] + p.vertical) continue;
            U32 first[2], last[2];
            bool intersects = true;
            for (U32 axis = 0; axis < 2; ++axis)
            {
                const F64 shift0 = p.shear[axis]*low[2], shift1 = p.shear[axis]*high[2];
                // One texel of guard includes filter/cull boundaries.
                const F64 pad = 2.*p.extent/p.resolution;
                const F64 a = (low[axis] + std::min(shift0, shift1) - p.center[axis] - pad)/p.extent;
                const F64 b = (high[axis] + std::max(shift0, shift1) - p.center[axis] + pad)/p.extent;
                if (a > 1. || b < -1.) { intersects = false; break; }
                first[axis] = U32(std::clamp(std::floor((a + 1.)*4.), 0., 7.));
                last[axis] = U32(std::clamp(std::floor((b + 1.)*4.), 0., 7.));
            }
            if (!intersects) continue;
            for (U32 y = first[1]; y <= last[1]; ++y)
                for (U32 x = first[0]; x <= last[0]; ++x)
                {
                    const U64 bit = U64(1) << (y*8 + x);
                    mDirty[layer] |= bit;
                    // Events can precede the frame's geometry flush. Retain
                    // one follow-up, just as WP7's original bounded scheduler.
                    mFollowup[layer] |= bit;
                }
        }
    }
    bool Updates::schedule(const Plan& close, const Plan& distant, U32 place,
        const std::array<F64, 3>& origin, F64 seconds, U32 hz, U32 layers)
    {
        if (!std::isfinite(seconds)) { invalidate(); mInitialized = false; return false; }
        const Plan requested[] = {close, distant};
        mLayerCount = layers;
        const bool urgent = !mInitialized || place != mPlace || origin != mOrigin || seconds < mLast;
        mLast = seconds;
        if (!urgent && seconds + 1e-9 < mNext) return false;
        for (U32 layer = 0; layer < layers; ++layer)
        {
            if (urgent || !(requested[layer] == mPlans[layer]) || !mValid)
            {
                mPlans[layer] = requested[layer];
                mComplete[layer] = false;
                mDirty[layer] = ~U64(0); mFollowup[layer] = 0;
            }
        }
        mInitialized = mValid = true; mPlace = place; mOrigin = origin;
        for (U32 offset = 0; offset < layers; ++offset)
        {
            const U32 layer = (mCursor + offset)%layers;
            if (!mDirty[layer]) continue;
            mSelected = layer; mCursor = (layer + 1)%layers;
            if (!mComplete[layer]) mTile = {};
            else
            {
                U32 bit = mTileCursor[layer];
                while (!(mDirty[layer] & (U64(1) << bit))) bit = (bit + 1)%64;
                mTileCursor[layer] = (bit + 1)%64;
                mTile = {bit%8, bit/8, 1};
                U32 low_x = 7, low_y = 7, high_x = 0, high_y = 0;
                for (U32 index = 0; index < 64; ++index)
                    if (mDirty[layer] & (U64(1) << index))
                    {
                        low_x = std::min(low_x, index%8); low_y = std::min(low_y, index/8);
                        high_x = std::max(high_x, index%8); high_y = std::max(high_y, index/8);
                    }
                const U32 span = std::max(high_x-low_x, high_y-low_y) + 1;
                // Coalesce a small moving caster's neighbouring tiles into one
                // cropped traversal. Widespread churn still gets one tile.
                if (span <= 4) mTile = {std::min(low_x, 8-span), std::min(low_y, 8-span), span};
                else mTile = {std::min(bit%8, 4U), std::min(bit/8, 4U), 4};
            }
            mNext = seconds + 1./std::clamp(hz, 5U, 60U);
            return true;
        }
        return false;
    }
    void Updates::complete()
    {
        U64 mask = 0;
        for (U32 y = mTile.y; y < mTile.y+mTile.size; ++y)
            for (U32 x = mTile.x; x < mTile.x+mTile.size; ++x) mask |= U64(1) << (y*8+x);
        mDirty[mSelected] = (mDirty[mSelected] & ~mask) | (mFollowup[mSelected] & mask);
        mFollowup[mSelected] &= ~mask;
        mComplete[mSelected] = true;
        // Finish initial paired publication immediately; subsequent dirty work
        // is round-robin, rate bounded and never builds both maps in a view.
        if (!mComplete[0] || (mLayerCount == 2 && (!mComplete[1] ||
            (mSelected == 0 && mDirty[1])))) mNext = 0.;
    }
    void Exposure::reset()
    {
        mExposure = 1.f;
        mLastPresentationTime = 0.0;
        mInitialized = false;
    }
    F32 Exposure::update(F64 presentation_time, F32 target)
    {
        target = std::clamp(target, 0.f, 1.f);
        if (!std::isfinite(presentation_time))
        {
            reset();
            return target;
        }
        if (!mInitialized || presentation_time < mLastPresentationTime)
        {
            mExposure = target;
            mLastPresentationTime = presentation_time;
            mInitialized = true;
            return mExposure;
        }
        // Preserve the existing main-view exponential response exactly.
        constexpr F64 RESPONSE_SECONDS = 0.2;
        const F64 elapsed = std::clamp(presentation_time - mLastPresentationTime, 0.0, 1.0);
        const F32 blend = static_cast<F32>(1.0 - std::exp(-elapsed / RESPONSE_SECONDS));
        mExposure = std::clamp(mExposure + (target - mExposure) * blend, 0.f, 1.f);
        if (std::abs(target - mExposure) <= 0.001f) mExposure = target;
        mLastPresentationTime = presentation_time;
        return mExposure;
    }
    Coverage classify(bool allocated, bool cleared, bool traversed, bool complete, U32 batches)
    {
        if (!allocated || !cleared) return Coverage::FAILED;
        if (!traversed || !complete) return Coverage::INCOMPLETE;
        return batches ? Coverage::COMPLETE_COVERED : Coverage::COMPLETE_EMPTY;
    }
    bool usable(Coverage state)
    {
        return state == Coverage::COMPLETE_EMPTY || state == Coverage::COMPLETE_COVERED;
    }
    const char* label(Coverage state)
    {
        switch (state)
        {
        case Coverage::COMPLETE_EMPTY: return "Open sky (complete empty cover)";
        case Coverage::COMPLETE_COVERED: return "Surface cover ready";
        case Coverage::FAILED: return "Surface cover failed; unknown surfaces stay dry";
        default: return "Surface cover incomplete; unknown surfaces stay dry";
        }
    }
    U64 materialSignature(const ALWeatherBridge::Controller& weather)
    {
        const auto& s = weather.settings();
        // Pure atmospheric rain never dirties a material probe, including mode 1.
        if (!weather.surfacesActive()) return 0;
        U64 hash = 1469598103934665603ULL;
        const auto add = [&hash](U64 n) { hash = (hash ^ n) * 1099511628211ULL; };
        const auto quantize = [](F32 n) { return U64(std::floor(std::clamp(n, 0.f, 1.f) * 16.f + 0.5f)); };
        add(1);
        add(quantize(weather.state().wetness));
        add(quantize(s.wetAmount));
        add(s.puddles ? quantize(s.puddleAmount) + 1 : 0);
        // Animated ripple phase/rain never forces recapture; configuration does.
        add(quantize(s.rippleStrength));
        add(s.coverResolution);
        add(U64(s.coverExtent));
        add(U64(s.wetSeconds));
        add(U64(s.drySeconds));
        add(quantize(s.startWetness));
        if (s.advancedShelter)
        {
            add(7);
            add(s.shelterNearResolution); add(s.shelterFarResolution);
            add(U64(s.shelterNearExtent * 1024.f)); add(U64(s.shelterFarExtent * 1024.f));
            add(U64(s.shelterBias * 1024.f)); add(U64(s.shelterSoftness * 1024.f));
            add(s.shelterTilt);
            if (s.shelterTilt)
            {
                const auto shear = fallShear(weather.state().windMean[0], weather.state().windMean[1],
                    weather.active() && s.snowEnabled && s.precipType == 1 ? s.snowSpeed : s.shelterFallSpeed, true);
                add(U64(S64(std::floor(shear[0] * 64.f + 0.5f))));
                add(U64(S64(std::floor(shear[1] * 64.f + 0.5f))));
            }
        }
        add(weather.placeSerial());
        add(weather.clearSerial());
        add(weather.surfaceSerial());
        return hash;
    }
    bool ProbeRevision::update(U64 signature, F64 seconds)
    {
        if (!std::isfinite(seconds)) return false;
        if (mValid && seconds < mLast) mNext = seconds;
        mLast = seconds;
        if (!mValid)
        {
            mValid = true;
            mPublished = signature;
            mNext = seconds + 0.5;
            return false;
        }
        if (signature == mPublished || seconds < mNext) return false;
        mPublished = signature;
        mNext = seconds + 0.5;
        return true;
    }
}
