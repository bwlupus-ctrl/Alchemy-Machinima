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
    bool Updates::due(const Plan& close, const Plan& distant, U64 geometry, U32 place,
                      const std::array<F64, 3>& origin, F64 seconds, U32 hz)
    {
        if (!std::isfinite(seconds)) { invalidate(); return false; }
        const bool new_geometry = geometry != mGeometry;
        const bool changed = !(close == mClose) || !(distant == mDistant) ||
            new_geometry || mSettle || place != mPlace || origin != mOrigin;
        const bool urgent = !mRequested || place != mPlace || origin != mOrigin || seconds < mLast;
        mLast = seconds;
        if (!urgent && ((!changed && mValid) || seconds < mNext)) return false;
        mRequested = true;
        // Native drawable revisions can precede the frame's geometry flush.
        // One bounded follow-up captures that completed transform/VBO update.
        mSettle = new_geometry;
        mClose = close; mDistant = distant; mGeometry = geometry; mPlace = place; mOrigin = origin;
        mNext = seconds + 1. / std::clamp(hz, 5U, 60U);
        mValid = false;
        return true;
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
