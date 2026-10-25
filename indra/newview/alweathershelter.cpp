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
