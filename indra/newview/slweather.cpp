/**
 * @file slweather.cpp
 * @brief Weather state (#783): see slweather.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Slipstream Viewer Source Code
 * (LGPL v2.1, see slweather.h)
 * $/LicenseInfo$
 * Alchemy adaptation (2026): injected config/time/place events; fixed-step caller;
 * preserve small snow deposits while accumulating; canonical teleport surfaces.
 */

#include "linden_common.h"

#include "slweather.h"


#include <algorithm>
#include <cmath>

// Alchemy adaptation: configuration, time and place events are injected. No viewer globals.
namespace {
constexpr F32 kWeatherRadians = 0.01745329251994329577f;
}

namespace
{
// Rain below this counts as "not raining" for the wetness (drying starts); snow below this
// as "not snowing" for the snow cover (melting starts).
constexpr F32 kWetRain = 0.02f;
constexpr F32 kSnowFalling = 0.02f;
// Rain on lying snow melts it this many times faster (at full rain).
constexpr F32 kRainMelt = 3.f;
// Water per unit of melted snow cover (wetness units): a melting blanket soaks the ground.
constexpr F32 kMeltWater = 1.5f;
// M4 RUN-OFF: melt water per unit of snow cover melting per second (a WeatherSnowMeltSeconds 900
// melt runs off at ~0.4), the rise time constant (s), and the wetness over which the run-off
// starts (the roofs soak first).
constexpr F32 kMeltRunoff = 400.f;
constexpr F32 kRunoffRise = 4.f;
constexpr F32 kRunoffWet0 = 0.1f, kRunoffWet1 = 0.4f;
// Longest step one update integrates (a stall or a minimised window must not jump a
// whole transition in one frame).
constexpr double kMaxStep = 0.5;
// PLACES (#895; slweather.h): a crossing / parcel change is judged once the region source has
// evaluated the destination, at the latest this long after it (frame clock, s); a teleport's
// seed waits this long for the destination's source.
constexpr F64 kMoveDecideSecs = 15.0;
constexpr F64 kSeedWindowSecs = 20.0;
// #783 SHARED SNOW DEPTH: a tag level at or below this (m) is bare ground (the cover melts
// naturally); above it the cover is held on.
constexpr F32 kSharedBare = 1e-4f;
// #954 PRECIPITATION RATE LADDER (slweather.h): rate = r0 x ((1 + top / r0)^amount - 1), so
// amount 0.5 lands on ~5 mm/h / ~1.5 cm/h (the old default intensity's look) and 1 on the top.
constexpr F32 kRainR0 = 100.f / 360.f;     // mm/h (amount 0.25 ~0.93, 0.75 ~23)
constexpr F32 kSnowR0 = 15.f / 80.f;       // cm/h (amount 0.25 ~0.38, 0.75 ~4.9)
// The classes' upper bounds (light / moderate / heavy; above: violent / blizzard-grade).
constexpr F32 kRainClass[3] = {2.5f, 7.6f, 50.f};
constexpr F32 kSnowClass[3] = {0.5f, 2.5f, 8.f};
// #954 WETNESS: WeatherWetSeconds is the soak time at this rain rate (mm/h); the soak speed scales
// with the rate within these bounds.
constexpr F32 kWetRefFall = 5.f;
constexpr F32 kWetSpeedMin = 0.05f, kWetSpeedMax = 6.f;
// #954 BANDS: the band and the cell (s per lattice step), the noise stretch, the strength, the
// downpour window (s), its chance at variation 1 and its bump.
constexpr F64 kBandSecs = 240.0, kCellSecs = 45.0;
constexpr F32 kBandGain = 1.1f;
constexpr F64 kBurstWindow = 1200.0;
constexpr F32 kBurstChance = 0.8f;
// #954 BLOWING SNOW: wind (m/s) over lying snow from which it lifts .. at which it is full.
constexpr F32 kBlowWind0 = 9.f, kBlowWind1 = 18.f;

// A tag's effective rate (m/h) for its level: growth only while the tag snows, a melting rate
// always; without since= there is no time base -- the depth holds. #954: no rate= but a snowcm=
// (and depth= / since=): the tag's mean snowfall x (1 - kSnowSettle). (A pure function of the tag.)
F32 tagLevelRate(const SLWeather::Target& t)
{
    if (!t.haveSince) return 0.f;
    if (!t.haveRate)
    {
        if (!t.haveSnowFall || !(t.snow > kSnowFalling)) return 0.f;
        return std::max(t.snowFall, 0.f) * (1.f - SLWeather::kSnowSettle) / 100.f;
    }
    if (t.snowRate > 0.f && !(t.snow > kSnowFalling)) return 0.f;
    return t.snowRate;
}

// A region source (parcel tags or the sky) is enabled: a move's destination is known only once
// it has evaluated the new place.

F32 ease(F32 cur, F32 target, F32 k)
{
    const F32 v = cur + (target - cur) * k;
    return std::fabs(v - target) < SLWeather::kEpsilon * 0.5f ? target : v;
}

F32 smooth01(F32 e0, F32 e1, F32 x)
{
    const F32 t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// ---- deterministic noise: a pure function of (seed, salt, position), the same on every
// viewer (integer hashing + IEEE float arithmetic only) ----
U64 mix64(U64 x)
{
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
U64 hash3(U32 seed, U32 salt, S64 k)
{
    return mix64((((U64)seed << 32) | salt) ^ mix64((U64)k + 0x9E3779B97F4A7C15ULL));
}
F32 u01(U64 h) { return (F32)(h >> 40) * (1.f / 16777216.f); }   // [0, 1), 24 bits
// Value noise -1..1, quintic between integer lattice points (C2: no visible kinks in a gust).
F32 vnoise(U32 seed, U32 salt, F64 x)
{
    const F64 fl = std::floor(x);
    const S64 i = (S64)fl;
    const F32 f = (F32)(x - fl);
    const F32 a = u01(hash3(seed, salt, i)) * 2.f - 1.f;
    const F32 b = u01(hash3(seed, salt, i + 1)) * 2.f - 1.f;
    const F32 w = f * f * f * (f * (f * 6.f - 15.f) + 10.f);
    return a + (b - a) * w;
}
enum : U32 { SALT_SWELL = 1, SALT_SURGE, SALT_GUST, SALT_DIR_SLOW, SALT_DIR_FAST,
             SALT_DIRV_SLOW = 0x10, SALT_DIRV_MID, SALT_DIRV_FAST,
             SALT_STRIKE = 0x100, SALT_GATE, SALT_TIME, SALT_DIST, SALT_AZ, SALT_ELEV, SALT_SHAPE, SALT_BITS,
             SALT_BAND = 0x200, SALT_CELL, SALT_BURST, SALT_BURST_T, SALT_BURST_W, SALT_BURST_A };

// ---- wind (slweather.h, WIND) ----
constexpr F64 kSwellHz = 1.0 / 45.0;
constexpr F64 kSurgeHz = 1.0 / 12.0;
constexpr F64 kGustHz  = 1.0 / 1.6;
constexpr F32 kNoiseGain = 1.35f;          // value noise sits mostly within +-0.6: stretch to the range
constexpr F32 kWanderDeg = 18.f;           // direction wander at variation 1
// Local weather's seed (any constant: only this viewer sees it).
constexpr U32 kLocalSeed = 0x783u;

// ---- lightning (slweather.h, LIGHTNING) ----
constexpr F64 kSlot = 1.0;                 // s of synced time per strike decision
constexpr F32 kMaxStrikeRate = 0.15f;      // strikes / s at storm 1 in full rain (~9 a minute within 10 km)
constexpr F32 kMinDist = 300.f, kMaxDist = 10000.f;
constexpr F64 kStrikeKeep = 40.0;          // s a strike stays listed after its flash (its thunder is long gone)
constexpr size_t kMaxStrikes = 16;
constexpr F32 kStormTau = 20.f;            // s: the storm builds / fades (strike gate)

F32 strikeRate(const SLWeather::Target& t)
{
    if (!t.active || t.storm <= 0.f) return 0.f;
    const F32 precip = std::clamp(t.rain + 0.3f * t.snow, 0.f, 1.f);
    return kMaxStrikeRate * std::clamp(t.storm, 0.f, 1.f) * (0.2f + 0.8f * precip);
}

// The flash at tau seconds after the strike: 2..4 return strokes, each a ~6 ms rise and a
// ~50 ms decay, the first the brightest; dimmer with distance.
F32 flashAt(const SLWeather::Strike& s, F32 tau)
{
    if (tau < 0.f || tau > s.duration + 0.4f) return 0.f;
    const U32 strokes = 2 + (s.hash >> 4) % 3;
    F32 best = 0.f;
    for (U32 i = 0; i < strokes; ++i)
    {
        const U64 h = mix64((U64)s.hash * 31u + i);
        const F32 at = i == 0 ? 0.f : s.duration * ((F32)i / (F32)(strokes - 1)) * (0.75f + 0.25f * u01(h));
        const F32 amp = i == 0 ? 1.f : 0.4f + 0.55f * u01(mix64(h));
        const F32 x = tau - at;
        if (x < 0.f) continue;
        const F32 p = x < 0.006f ? x / 0.006f : std::exp(-(x - 0.006f) / 0.05f);
        best = std::max(best, amp * p);
    }
    const F32 distFade = std::sqrt(std::clamp((s.dist - kMinDist) / (kMaxDist - kMinDist), 0.f, 1.f));
    return std::clamp(best * (1.f - 0.7f * distFade), 0.f, 1.f);
}
} // namespace

// static
F64 SLWeather::syncedSeconds() const
{
    return mInput.syncSeconds;
}

bool SLWeather::regionSourceOn() const
{
    return mConfig.regionSource;
}

// static
F32 SLWeather::precipFog(F32 /*rain*/, F32 /*snow*/)
{
    // #954 (owner feedback): none -- the precipitation's own medium is State::precipExt (one
    // physical source, injected into the volumetric fog where it falls, or the renderer's
    // analytic fallback); a source's explicit fog= (a tag) remains an extra fog on top.
    return 0.f;
}

// ---- #954 PRECIPITATION RATE (slweather.h) ----
// static
F32 SLWeather::amountFromFall(EPrecip type, F32 fall)
{
    const bool snow = type == PRECIP_SNOW;
    const F32 r0 = snow ? kSnowR0 : kRainR0, top = snow ? kSnowFallTop : kRainFallTop;
    if (!(fall > 0.f)) return 0.f;
    return std::log1p(fall / r0) / std::log1p(top / r0);
}

// static
F32 SLWeather::fallFromAmount(EPrecip type, F32 amount)
{
    const bool snow = type == PRECIP_SNOW;
    const F32 r0 = snow ? kSnowR0 : kRainR0, top = snow ? kSnowFallTop : kRainFallTop;
    if (!(amount > 0.f)) return 0.f;
    return r0 * std::expm1(amount * std::log1p(top / r0));
}

// static
U32 SLWeather::fallClass(EPrecip type, F32 fall)
{
    const F32* c = type == PRECIP_SNOW ? kSnowClass : kRainClass;
    U32 k = 0;
    while (k < 3 && fall >= c[k]) ++k;
    return k;
}

void SLWeather::Target::setFall(EPrecip type, F32 fall)
{
    const F32 top = type == PRECIP_SNOW ? kSnowFallMax : kRainFallMax;
    const F32 f = std::clamp(std::isfinite(fall) ? fall : 0.f, 0.f, top);
    const F32 a = std::min(amountFromFall(type, f), 1.f);
    if (type == PRECIP_SNOW) { snowFall = f; snow = a; }
    else { rainFall = f; rain = a; }
}

// static
F32 SLWeather::fallVariationAt(U32 seed, F32 variation, F64 syncNow)
{
    const F32 var = std::clamp(variation, 0.f, 1.f);
    if (var <= 0.f) return 1.f;
    // The band (~4 min) and the cell (~45 s): multiplicative, so a heavy band is x3, not + a constant.
    const F32 n = std::clamp(kNoiseGain * (0.65f * vnoise(seed, SALT_BAND, syncNow / kBandSecs)
                                        + 0.35f * vnoise(seed, SALT_CELL, syncNow / kCellSecs)), -1.f, 1.f);
    F32 m = std::exp(kBandGain * var * n);
    // A DOWNPOUR / SQUALL passing: in a share of the 20-minute windows (more with the variation), a
    // bump centred inside the window, ~1-2.5 minutes wide; the neighbouring windows' tails count.
    const S64 w = (S64)std::floor(syncNow / kBurstWindow);
    F32 burst = 0.f;
    for (S64 k = w - 1; k <= w + 1; ++k)
    {
        if (u01(hash3(seed, SALT_BURST, k)) >= kBurstChance * var) continue;
        const F64 centre = ((F64)k + 0.2 + 0.6 * (F64)u01(hash3(seed, SALT_BURST_T, k))) * kBurstWindow;
        const F64 width = 60.0 + 90.0 * (F64)u01(hash3(seed, SALT_BURST_W, k));
        const F64 x = (syncNow - centre) / width;
        if (std::fabs(x) > 4.0) continue;
        const F32 amp = 0.6f + 1.0f * u01(hash3(seed, SALT_BURST_A, k));
        burst = std::max(burst, amp * (F32)std::exp(-x * x));
    }
    m *= 1.f + var * burst;
    return std::clamp(m, 0.2f, 4.f);
}

// static
F32 SLWeather::precipExtinction(F32 rainFall, F32 snowFall, F32 blowing)
{
    // Koschmieder-style fits (visibility ~3 / extinction): rain ~3 km at 2 mm/h, ~300 m at 100 mm/h;
    // snow ~900 m at 1.5 cm/h, ~140 m at 15 cm/h; blowing snow adds up to ~0.035 / m with the
    // snowfall (a ground blizzard alone ~0.012 / m: ~250 m).
    const F32 r = rainFall > 0.f ? 3.0e-4f * std::pow(rainFall, 0.75f) : 0.f;
    snowFall = std::min(snowFall, kSnowFallLookMax);   // past a heavy blizzard only the build-up grows
    const F32 s = snowFall > 0.f ? 2.5e-3f * std::pow(snowFall, 0.8f) : 0.f;
    const F32 b = std::clamp(blowing, 0.f, 1.f) * (0.012f + 0.025f * smooth01(0.f, 5.f, snowFall));
    return r + s + b;
}

// static
F32 SLWeather::snowHeatMelt(const Target& /*t*/, F64 /*since*/, F64 /*syncNow*/)
{
    // #949 (fires melting snow): the deterministic replay of the heat sources goes here -- the
    // metres of open-sky snow they melted between `since` and `syncNow`, a pure function of what
    // every viewer on the parcel sees (the tag, the sources' positions / start times on synced
    // UTC). Not implemented: no heat source melts the shared level yet.
    return 0.f;
}

// static
F32 SLWeather::snowLevelAt(const Target& t, F64 syncNow) const
{
    const F32 s_max = mConfig.maxDepth;
    const bool s_coat = mConfig.snowCoating;
    if (!t.haveDepth) return -1.f;
    const F32 cap = s_coat ? std::clamp((F32)s_max, 0.f, kSnowDepthMax) : 0.f;
    if (cap <= 0.f) return 0.f;
    const F64 since = t.haveSince ? t.snowSince : syncNow;
    // (a since= ahead of this viewer's clock -- a stamp from a viewer whose clock runs ahead --
    // holds the depth until then instead of extrapolating backwards)
    const F64 elapsed = std::max(syncNow - since, 0.0);
    const F64 level = (F64)t.snowDepthAt + (F64)tagLevelRate(t) / 3600.0 * elapsed - (F64)snowHeatMelt(t, since, syncNow);
    return (F32)std::clamp(level, 0.0, (F64)cap);
}

SLWeather& SLWeather::instance()
{
    static SLWeather s;
    return s;
}

bool SLWeather::masterEnabled() const
{
    return mConfig.master;
}

SLWeather::Target SLWeather::localTarget() const
{
    return mConfig.local;
}

// The source layering (slweather.h): the local choice, else the region's (only ever set
// with the user's opt-in), else nothing.
SLWeather::Target SLWeather::resolve(ESource& source) const
{
    const Target local = localTarget();
    const bool master = masterEnabled();   // (the master gate: off = no source counts)
    if (master && local.active)
    {
        source = SOURCE_LOCAL;
        return local;
    }
    if (master && mHaveRegion && mRegion.active)
    {
        source = SOURCE_REGION;
        return mRegion;
    }
    source = SOURCE_NONE;
    Target off;
    off.transitionSecs = local.transitionSecs;   // (ease out at the user's pace)
    off.windVariation = mTarget.windVariation;   // (the gusts keep their character while dying down)
    off.windGustiness = mTarget.windGustiness;
    off.windDirVariation = mTarget.windDirVariation;
    off.seed = mTarget.seed;
    return off;
}

void SLWeather::update(const Config& config, const Input& input)
{
    if (!std::isfinite(input.seconds) || !std::isfinite(input.syncSeconds) ||
        input.seconds < 0.0 || std::fabs(input.syncSeconds) > 1.e12 || input.tick == mLastFrame) return;
    mConfig = config;
    mInput = input;
    mLastFrame = input.tick;
    const double now = input.seconds;
    const double dt = mLastTime < 0.0 ? 0.0 : std::clamp(now - mLastTime, 0.0, kMaxStep);
    mLastTime = now;

    // #904: a reset asked for (requestReset).
    if (const char* why = mPendingReset.exchange(nullptr)) applyReset(why);
    // #895 PLACES: a teleport far enough is a new place (slweather.h).
    if (input.teleportSucceeded) newPlace("successful teleport", 0.0);
    else if (input.regionHandle != mRegionHandle) onRegionChanged();
    // #904 MASTER OFF = everything gone at once: no easing out, no melting / drying / draining
    // over minutes -- every accumulated amount (snow cover and depth, wetness, run-off, the wind,
    // a flash, the pending strikes) is reset on the off edge (resetSerial: the renderer destroys its
    // weather module this recording -- grids, tracks, avatar snow, shelter maps, run-off, particles
    // -- and the audio cuts its beds and thunder), and the state is held empty while off.
    const bool master = masterEnabled();
    if (!master && mMasterOn) applyReset("the weather master switch went off");
    mMasterOn = master;

    ESource source = SOURCE_NONE;
    const Target t = resolve(source);
    mTarget = t;
    mState.source = source;
    if (!master)
    {
        mStrikes.clear();
        mState = State{};
        mState.source = source;
        mRunoffNear = 0.f;
        mListenerOpen = -1.f;
        return;
    }
    if (source == SOURCE_REGION) mRegionAccum = true;
    const F64 syncNow = syncedSeconds();

    // #895 PLACES (slweather.h): a teleport's seed once the destination's source is known, a
    // crossing / parcel change's decision once its destination is.
    if (mSeedPending)
    {
        if (source == SOURCE_LOCAL || mSeedSettled || !regionSourceOn())
        {
            mSeedPending = false;
            seedPlace(t, source);
        }
        else if (now >= mSeedDeadline)
        {
            mSeedPending = false;
        }
    }
    if (mMoveWhy && (mMoveSettled || !regionSourceOn() || source == SOURCE_LOCAL || now >= mMoveDeadline))
        decideMove(t, source);

    // Ease toward the target: time constant = a third of the transition (0 = immediate).
    const F32 tau = t.transitionSecs / 3.f;
    const F32 k = tau <= 1e-3f ? 1.f : (F32)(1.0 - std::exp(-dt / (double)tau));
    // #954 BANDS (slweather.h): the precipitation eases toward the VARIED rate (in amount space,
    // unclamped above the ladder's top); everything else below reads the unvaried target.
    {
        const F32 m = t.active ? fallVariationAt(t.seed, t.fallVariation, syncNow) : 1.f;
        const F32 ra = ease(amountFromFall(PRECIP_RAIN, mState.rainFall), amountFromFall(PRECIP_RAIN, t.rainFall * m), k);
        const F32 sa = ease(amountFromFall(PRECIP_SNOW, mState.snowFall), amountFromFall(PRECIP_SNOW, t.snowFall * m), k);
        mState.rainFall = fallFromAmount(PRECIP_RAIN, ra);
        mState.snowFall = fallFromAmount(PRECIP_SNOW, sa);
        mState.rain = std::min(ra, 1.f);
        mState.snow = std::min(sa, 1.f);
    }
    mState.fog = ease(mState.fog, t.fog, k);
    mState.cloud = ease(mState.cloud, t.cloud, k);
    // Wind as a vector: "from" direction theta (clockwise from north) blows toward theta + 180.
    {
        const F32 a = t.windDirDeg * kWeatherRadians;
        const F32 tx = t.active ? -std::sin(a) * t.windSpeed : 0.f;
        const F32 ty = t.active ? -std::cos(a) * t.windSpeed : 0.f;
        mState.windMean[0] = mState.windMean[0] + (tx - mState.windMean[0]) * k;
        mState.windMean[1] = mState.windMean[1] + (ty - mState.windMean[1]) * k;
    }
    stepWind(t, syncNow);
    stepLightning(t, (F32)dt, syncNow, now);

    // Snow cover: builds while it snows (faster in heavier snow), melts after -- faster in
    // rain. The melted amount is water (below).
    const F32 s_build = mConfig.buildSeconds;
    const F32 s_melt = mConfig.meltSeconds;
    const bool s_coat = mConfig.snowCoating;
    // #783 SHARED SNOW DEPTH: the parcel tag's level (-1: none / not the region source).
    const F32 sharedLevel = (source == SOURCE_REGION && t.active) ? snowLevelAt(t, syncNow) : -1.f;
    const bool sharedHeld = sharedLevel > kSharedBare;   // (a held / growing shared depth: the cover stays on)
    // (this place was evaluated without a shared depth: a tag met later is a move onto it -- faded)
    // (and with the region source off at a known place nothing tag-driven can arrive: a later opt-in
    // onto a tagged parcel is a re-sync, faded, not an arrival)
    if (sharedLevel < 0.f && (mSettledHandle != 0 || (!regionSourceOn() && mRegionHandle != 0))) mSyncArrival = false;
    F32 melted = 0.f;
    if (!s_coat)
        mState.snowCover = 0.f;
    else if (mState.snow > kSnowFalling)
    {
        const F32 rate = (F32)dt / std::max((F32)s_build, 1.f) * std::min(mState.snow * 2.f, 1.f);
        mState.snowCover = std::min(mState.snowCover + rate, 1.f);
    }
    else if (mState.snowCover > 0.f && !sharedHeld)
    {
        const F32 rate = (F32)dt / std::max((F32)s_melt, 1.f) * (1.f + kRainMelt * std::min(mState.rain * 2.f, 1.f));
        melted = std::min(rate, mState.snowCover);
        mState.snowCover -= melted;
        if (mState.snowCover < kEpsilon) mState.snowCover = 0.f;
    }
    // M3 snow depth (slweather.h SNOW DEPTH): metres, beside the cover, capped by it -- or (#783
    // SHARED SNOW DEPTH) the parcel tag's computed level.
    {
        const F32 s_max = mConfig.maxDepth;
        const F32 maxD = std::clamp((F32)s_max, 0.f, kSnowDepthMax);
        const bool falling = mState.snow > kSnowFalling;
        // The rates (m/s): #954 the snowfall rate falling now (its bands included) x (1 - the
        // settling); a tag's rate= overrides it for the depth (the snowfall still drives the falling
        // flakes); a tag depth= without a usable rate holds.
        F32 build = 0.f, melt = 0.f;
        if (sharedLevel >= 0.f)
        {
            const F32 r = tagLevelRate(t);
            build = std::max(r, 0.f) / 3600.f;
            melt = std::max(-r, 0.f) / 3600.f;
        }
        else
        {
            const bool tagRate = source == SOURCE_REGION && t.active && t.haveRate;
            if (falling)
                build = tagRate ? std::max(t.snowRate, 0.f) / 3600.f
                                : std::max(mState.snowFall, 0.f) * (1.f - kSnowSettle) / (100.f * 3600.f);
            if (tagRate && t.snowRate < 0.f) melt = -t.snowRate / 3600.f;
            else if (!falling) melt = maxD / std::max((F32)s_melt, 1.f) * (1.f + kRainMelt * std::min(mState.rain * 2.f, 1.f));
        }
        if (!s_coat || maxD <= 0.f)
        {
            mState.snowDepth = 0.f;
            build = melt = 0.f;
            mSyncActive = false;
        }
        else if (sharedLevel >= 0.f)
            stepDepthSync(t, source, sharedLevel, (F32)dt, syncNow);
        else
        {
            if (mSyncActive)
            {
                mSyncActive = false;
                mSyncFadeLeft = 0.f;
            }
            mState.snowDepth += (F32)dt * (build - melt);
            mState.snowDepth = std::clamp(mState.snowDepth, 0.f, maxD * mState.snowCover);
            // Keep sub-threshold deposits while building; upstream erased every fresh deposit.
            if (build <= melt && mState.snowDepth < 1e-4f) mState.snowDepth = 0.f;
        }
        mState.snowDepthBuild = build;
        mState.snowDepthMelt = melt;
        mState.snowDepthShared = mSyncActive;

    }

    // Wetness: soaks up while it rains (faster in heavier rain) and while lying snow melts;
    // dries after (not while there is still snow melting into it).
    const F32 s_wet = mConfig.wetSeconds;
    const F32 s_dry = mConfig.drySeconds;
    const bool s_surf = mConfig.wetSurfaces;
    if (!s_surf)
        mState.wetness = 0.f;
    else
    {
        const bool raining = mState.rain > kWetRain;
        if (raining)
        {
            // #954: by the rain rate (WeatherWetSeconds at kWetRefFall; a downpour soaks in seconds).
            const F32 speed = std::clamp(mState.rainFall / kWetRefFall, kWetSpeedMin, kWetSpeedMax);
            const F32 rate = (F32)dt / std::max((F32)s_wet, 1.f) * speed;
            mState.wetness = std::min(mState.wetness + rate, 1.f);
        }
        if (melted > 0.f)
            mState.wetness = std::min(mState.wetness + melted * kMeltWater, 1.f);
        if (!raining && mState.snowCover <= 0.f)
        {
            mState.wetness = std::max(mState.wetness - (F32)dt / std::max((F32)s_dry, 1.f), 0.f);
            if (mState.wetness < kEpsilon) mState.wetness = 0.f;
        }
    }

    // M4 RUN-OFF: no accumulated runoff before its separately enabled consumer (WP8).
    if (!mConfig.runoff) mState.runoff = 0.f;
    else
    {
        const F32 s_drain = mConfig.drainSeconds;
        F32 src = mState.rain > kWetRain ? mState.rain : 0.f;
        if (dt > 0.0 && melted > 0.f) src += std::min(melted / (F32)dt * kMeltRunoff, 1.f);
        const F32 gate = s_surf ? smooth01(kRunoffWet0, kRunoffWet1, mState.wetness) : 1.f;
        const F32 target = std::clamp(src * gate, 0.f, 1.f);
        const F32 tau = target >= mState.runoff ? kRunoffRise : std::max((F32)s_drain / 3.f, 1.f);
        if (dt > 0.0) mState.runoff += (target - mState.runoff) * (F32)(1.0 - std::exp(-dt / (double)tau));
        if (s_surf) mState.runoff = std::min(mState.runoff, mState.wetness);
        // (snapped to zero only while draining: a light shower's first steps are below epsilon)
        if (target <= 0.f && mState.runoff < kEpsilon) mState.runoff = 0.f;
        mState.runoff = std::clamp(mState.runoff, 0.f, 1.f);
    }
    // #895: the fade after a move to a place without snow / wet weather (slweather.h PLACES).
    stepFade(t, (F32)dt);
    // #954 VISIBILITY (slweather.h): the blowing snow (the gusty wind over lying snow) and the
    // precipitation's own extinction, x WeatherFogAmount.
    {
        const F32 s_fogAmt = mConfig.fogAmount;
        const F32 wind = std::sqrt(mState.wind[0] * mState.wind[0] + mState.wind[1] * mState.wind[1]);
        mState.blowing = smooth01(kBlowWind0, kBlowWind1, wind) * std::min(mState.snowCover * 2.f, 1.f);
        if (mState.blowing < kEpsilon) mState.blowing = 0.f;
        const F32 amt = std::clamp((F32)s_fogAmt, 0.f, 4.f);
        mState.precipExt = precipExtinction(mState.rainFall, mState.snowFall, mState.blowing) * amt;
        mState.precipExtBlow = std::max(mState.precipExt - precipExtinction(mState.rainFall, mState.snowFall, 0.f) * amt, 0.f);
    }
    // (all of the region's weather gone by itself: a move has nothing of it to fade)
    if (source != SOURCE_REGION && !mState.visible()) mRegionAccum = false;
}

// ---- RESET (#904; slweather.h PLACES) ----
void SLWeather::requestReset(const char* why)
{
    mPendingReset.store(why ? why : "requested");
}

void SLWeather::applyReset(const char* /*why*/)
{
    // A pending reset coincident with the master-off edge is one clean slate.
    mMasterOn = mConfig.master;
    mStrikes.clear();
    mSlotValid = false;      // (strike slots restart at the present)
    mStormEase = 0.f;        // (a storm at the new place builds up again)
    mState = State{};
    mTarget = Target{};
    mHaveRegion = false;     // (the region source re-feeds only what the NEW parcel publishes)
    mRegion = Target{};
    mRegionAccum = false;
    mRunoffNear = 0.f;
    mListenerOpen = -1.f;
    mMoveWhy = nullptr;
    mSeedPending = mSeedSettled = false;
    mFadeLeft = 0.f;
    mFadeSnow = mFadeWet = false;
    mSettledHandle = 0;
    mSettledParcel = -1;
    mSyncActive = false;     // (#783: the next shared depth is an arrival: set at once)
    mSyncArrival = true;
    mSyncFadeLeft = 0.f;
    mClearSerial.fetch_add(1, std::memory_order_acq_rel);   // (the renderer frees its whole module)
    mResetSerial.fetch_add(1, std::memory_order_acq_rel);   // (audio cut; region source re-polls)
}

// ---- PLACES (#895; slweather.h) ----
void SLWeather::onRegionChanged()
{
    const U64 previous = mRegionHandle;
    mRegionHandle = mInput.regionHandle;
    if (!previous) return;
    mMoveWhy = "region crossing";
    mMoveDeadline = mInput.seconds + kMoveDecideSecs;
    mMoveSettled = false;
}

void SLWeather::noteSourceSettled(U64 regionHandle, S32 parcelId)
{
    if (regionHandle == mSettledHandle && parcelId == mSettledParcel) return;
    const bool first = mSettledHandle == 0;
    const bool otherRegion = regionHandle != mSettledHandle;
    mSettledHandle = regionHandle;
    mSettledParcel = parcelId;
    if (mSeedPending)
    {
        mSeedSettled = true;   // (a teleport's destination: seeded by update())
        return;
    }
    if (first) return;   // (login / after a teleport: no move; inside one: watchTeleport decides)
    if (!mMoveWhy)
    {
        mMoveWhy = otherRegion ? "region crossing" : "parcel change";
        mMoveDeadline = mInput.seconds + kMoveDecideSecs;
    }
    mMoveSettled = true;
}

void SLWeather::setSurfaceWetness(F32 wetness)
{
    mState.wetness = mConfig.master && mConfig.wetSurfaces && std::isfinite(wetness)
        ? std::clamp(wetness, 0.f, 1.f) : 0.f;
    mState.runoff = std::min(mState.runoff, mState.wetness);
}

void SLWeather::newPlace(const char* /*why*/, F64 /*dist*/)
{
    // Every SUCCESSFUL teleport resets patterns/surfaces, including short local hops.
    // Keep the configured storm target; crossings and origin rebases never enter here.
    mPlaceSerial.fetch_add(1, std::memory_order_acq_rel);
    mResetSerial.fetch_add(1, std::memory_order_acq_rel);
    mRegionHandle = mInput.regionHandle;
    mSettledHandle = 0;
    mSettledParcel = -1;
    mHaveRegion = false;
    mRegion = Target{};
    mRegionAccum = false;
    mMoveWhy = nullptr;
    mFadeLeft = 0.f;
    mFadeSnow = mFadeWet = false;
    mSyncActive = false;
    mSyncArrival = true;
    mSyncFadeLeft = 0.f;
    mStrikes.clear();
    mSlotValid = false;
    mStormEase = 0.f;
    mState = State{};
    mState.wetness = mConfig.wetSurfaces ? std::clamp(mConfig.startWetness, 0.f, 1.f) : 0.f;
    mState.snowDepth = mConfig.snowCoating ? std::clamp(mConfig.startDepth, 0.f, mConfig.maxDepth) : 0.f;
    mState.snowCover = mState.snowDepth > 0.f ? 1.f : 0.f;
    mRunoffNear = 0.f;
    mListenerOpen = -1.f;
    mSeedPending = mConfig.regionSource && !mConfig.local.active;
    mSeedSettled = false;
    mSeedDeadline = mInput.seconds + kSeedWindowSecs;
}

// A teleport's destination at its steady state (slweather.h PLACES): snowing -> the full cover and
// WeatherSnowMaxDepth x the intensity (it builds on from there), raining -> wet; the precipitation
// itself at its target (no ease-in: one arrives in the weather). Nothing falls there: nothing.
void SLWeather::seedPlace(const Target& t, ESource source)
{
    const F32 s_max = mConfig.maxDepth;
    const bool s_coat = mConfig.snowCoating;
    const bool s_surf = mConfig.wetSurfaces;
    // #783 SHARED SNOW DEPTH: a parcel tag's depth is the destination's level (update() applies it
    // at once: an arrival), whether or not anything falls there.
    const F32 shared = source == SOURCE_REGION ? snowLevelAt(t, syncedSeconds()) : -1.f;
    if (!t.active || (t.snow <= kSnowFalling && t.rain <= kWetRain && shared <= kSharedBare))
    {
        return;
    }
    mState.rain = t.rain;
    mState.snow = t.snow;
    mState.rainFall = t.rainFall;   // (#954: the mean rate -- the bands take over from the next frame)
    mState.snowFall = t.snowFall;
    mState.fog = t.fog;
    mState.cloud = t.cloud;
    const F32 a = t.windDirDeg * kWeatherRadians;
    mState.windMean[0] = -std::sin(a) * t.windSpeed;
    mState.windMean[1] = -std::cos(a) * t.windSpeed;
    const F32 maxD = std::clamp((F32)s_max, 0.f, kSnowDepthMax);
    if (s_coat && shared >= 0.f)
    {
        mState.snowDepth = shared;
        if (shared > kSharedBare || t.snow > kSnowFalling) mState.snowCover = 1.f;
    }
    else if (s_coat && t.snow > kSnowFalling)
    {
        mState.snowCover = 1.f;
        mState.snowDepth = std::clamp(mConfig.startDepth, 0.f, maxD);
    }
    if (s_surf) mState.wetness = std::clamp(mConfig.startWetness, 0.f, 1.f);
    if (source == SOURCE_REGION) mRegionAccum = true;
}

// A crossing / parcel change, its destination known (or the wait over): keep or fade.
void SLWeather::decideMove(const Target& t, ESource source)
{
    const F32 s_fade = mConfig.crossingFadeSeconds;
    const char* why = mMoveWhy;
    mMoveWhy = nullptr;
    mMoveSettled = false;
    const bool snowLies = mState.snowCover > kEpsilon || mState.snowDepth > 0.f;
    const bool wetLies = mState.wetness > kEpsilon || mState.runoff > kEpsilon;
    if (source == SOURCE_LOCAL)
    {
        mFadeLeft = 0.f;
        mFadeSnow = mFadeWet = false;
        return;
    }
    if (!snowLies && !wetLies)
    {
        return;
    }
    if (!mRegionAccum)
    {
        return;
    }
    // (#783: a parcel tag's shared depth above bare ground is snow lying there -- its re-sync fades
    // the depth to the tag's level, stepDepthSync)
    const bool snowThere = source == SOURCE_REGION && t.active
                        && (t.snow > kSnowFalling || snowLevelAt(t, syncedSeconds()) > kSharedBare);
    const bool wetThere = snowThere || (source == SOURCE_REGION && t.active && t.rain > kWetRain);
    const bool fadeSnow = snowLies && !snowThere;
    const bool fadeWet = wetLies && !wetThere;
    const char* dest = snowThere ? "destination snowy" : wetThere ? "destination raining, no snow"
                     : source == SOURCE_REGION ? "destination clear" : "destination has no weather";
    if (!fadeSnow && !fadeWet)
    {
        mFadeLeft = 0.f;
        mFadeSnow = mFadeWet = false;
        return;
    }
    const F32 secs = std::clamp((F32)s_fade, 0.f, 600.f);
    const char* what = fadeSnow && fadeWet ? "snow and wetness" : fadeSnow ? "snow (wetness kept)" : "wetness (snow kept)";
    if (secs <= 0.f)
    {
        return;
    }
    mFadeSnow = fadeSnow;
    mFadeWet = fadeWet;
    mFadeLeft = mFadeLeft > 0.f ? std::min(mFadeLeft, secs) : secs;   // (a parcel change mid-fade never prolongs it)
}

// The move's fade: linear to zero over what is left of WeatherCrossingFadeSeconds (the renderer's
// grids are capped by the global cover / depth, so they fade with it). Snow falling again (or rain,
// for the wetness) ends it: the weather there takes over.
void SLWeather::stepFade(const Target& t, F32 dt)
{
    if (mFadeLeft <= 0.f) return;
    if (t.active && t.snow > kSnowFalling) mFadeSnow = false;
    if (t.active && t.haveDepth && snowLevelAt(t, syncedSeconds()) > kSharedBare) mFadeSnow = false;   // (#783: the shared level)
    if (t.active && (t.rain > kWetRain || t.snow > kSnowFalling)) mFadeWet = false;
    if (!mFadeSnow && !mFadeWet)
    {
        mFadeLeft = 0.f;
        return;
    }
    if (dt <= 0.f) return;
    const F32 k = dt >= mFadeLeft ? 1.f : dt / mFadeLeft;
    if (mFadeSnow)
    {
        mState.snowCover -= mState.snowCover * k;
        mState.snowDepth -= mState.snowDepth * k;
        if (mState.snowCover < kEpsilon) mState.snowCover = 0.f;
        if (mState.snowDepth < 1e-4f) mState.snowDepth = 0.f;
    }
    if (mFadeWet)
    {
        mState.wetness -= mState.wetness * k;
        mState.runoff -= mState.runoff * k;
        if (mState.wetness < kEpsilon) mState.wetness = 0.f;
        if (mState.runoff < kEpsilon) mState.runoff = 0.f;
    }
    mFadeLeft -= dt;
    if (mFadeLeft <= 0.f)
    {
        mFadeLeft = 0.f;
        mFadeSnow = mFadeWet = false;
    }
}

// #783 SHARED SNOW DEPTH (slweather.h): the tag's computed level drives snowDepth. A new tag (or a
// first application) logs and, unless it is an ARRIVAL, starts a re-sync fade from what lies now;
// after the fade the depth IS the level every frame (no integration: a stall, a minimised window or
// another frame rate cannot put this viewer out of step). Footprints, tracks, drifts and compaction
// are the renderer's own, layered on this level.
void SLWeather::stepDepthSync(const Target& t, ESource source, F32 level, F32 dt, F64 syncNow)
{
    const F32 s_fade = mConfig.syncFadeSeconds;
    const F32 s_max = mConfig.maxDepth;
    const bool snowing = t.snow > kSnowFalling;
    // (#954: the effective rate -- a snowcm= change re-syncs like a rate= change)
    const bool changed = !mSyncActive || t.snowDepthAt != mSyncDepth || tagLevelRate(t) != mSyncRate || t.haveSince != mSyncHaveSince
                      || (t.haveSince && t.snowSince != mSyncSince) || snowing != mSyncSnowing;
    if (changed)
    {
        const bool arrival = mSyncArrival;
        const F32 secs = arrival ? 0.f : std::clamp((F32)s_fade, 0.f, 120.f);
        mSyncFromDepth = mState.snowDepth;
        mSyncFromCover = mState.snowCover;
        mSyncFadeTotal = mSyncFadeLeft = secs;
        mSyncActive = true;
        mSyncArrival = false;
        mSyncDepth = t.snowDepthAt;
        mSyncRate = tagLevelRate(t);
        mSyncSince = t.snowSince;
        mSyncHaveSince = t.haveSince;
        mSyncSnowing = snowing;
        const F32 cap = std::clamp((F32)s_max, 0.f, kSnowDepthMax);
        const F32 rate = tagLevelRate(t);
        const F64 elapsed = t.haveSince ? std::max(syncNow - t.snowSince, 0.0) : 0.0;
        const bool capped = (F64)t.snowDepthAt + (F64)rate / 3600.0 * elapsed > (F64)cap + 1e-4;
    }
    F32 a = 1.f;
    if (mSyncFadeLeft > 0.f)
    {
        mSyncFadeLeft = std::max(mSyncFadeLeft - std::max(dt, 0.f), 0.f);
        const F32 x = mSyncFadeTotal > 0.f ? 1.f - mSyncFadeLeft / mSyncFadeTotal : 1.f;
        a = x * x * (3.f - 2.f * x);
    }
    mState.snowDepth = mSyncFromDepth + (level - mSyncFromDepth) * a;
    if (mState.snowDepth < 1e-4f) mState.snowDepth = 0.f;
    // The cover: held on while the shared level lies (a held depth never melts with the natural
    // cover); bare ground: the natural cover (update(): it melts when nothing falls).
    if (level > kSharedBare) mState.snowCover = std::max(mState.snowCover, mSyncFromCover + (1.f - mSyncFromCover) * a);
}

// The gusts on top of the eased mean (slweather.h, WIND).
void SLWeather::stepWind(const Target& t, F64 syncNow)
{
    const F32 var = std::clamp(t.windVariation, 0.f, 1.f);
    const F32 g = std::clamp(t.windGustiness, 0.f, 1.f);
    const U32 seed = t.seed;
    F32 m = 0.f, wander = 0.f;
    if (var > 0.f)
    {
        const F32 swell = vnoise(seed, SALT_SWELL, syncNow * kSwellHz);
        const F32 surge = vnoise(seed, SALT_SURGE, syncNow * kSurgeHz);
        const F32 gust = vnoise(seed, SALT_GUST, syncNow * kGustHz);
        // 0: slow swells (the swell and the surge) .. 1: sharp gusts riding a weaker swell.
        m = kNoiseGain * ((0.7f - 0.3f * g) * swell + 0.5f * (1.f - g) * surge + 0.75f * g * gust);
        m = std::clamp(m, -1.f, 1.f);
        wander = var * kWanderDeg * kWeatherRadians
                 * (0.7f * vnoise(seed, SALT_DIR_SLOW, syncNow * kSwellHz * 0.7) + 0.3f * vnoise(seed, SALT_DIR_FAST, syncNow * kSurgeHz));
    }
    const F32 scale = std::max(1.f + var * m, 0.f);
    const F32 mx = mState.windMean[0], my = mState.windMean[1];
    // The direction swing (WeatherWindDirVariation): independent of the speed variation, so a
    // steady-speed wind can still veer. Slow veers, a ~12 s shift, and (with the gustiness) quick
    // gust veers; +-dirVar degrees at the extremes of the noise.
    F32 veer = 0.f;
    const F32 dv = std::clamp(t.windDirVariation, 0.f, 180.f);
    if (dv > 0.f)
    {
        const F32 slow = vnoise(seed, SALT_DIRV_SLOW, syncNow * kSwellHz * 0.8);
        const F32 mid = vnoise(seed, SALT_DIRV_MID, syncNow * kSurgeHz);
        const F32 fast = vnoise(seed, SALT_DIRV_FAST, syncNow * kGustHz);
        const F32 n = std::clamp(kNoiseGain * ((0.85f - 0.45f * g) * slow + 0.3f * mid + 0.4f * g * fast), -1.f, 1.f);
        veer = dv * kWeatherRadians * n;
    }
    const F32 c = std::cos(wander + veer), sn = std::sin(wander + veer);
    mState.wind[0] = (mx * c - my * sn) * scale;
    mState.wind[1] = (mx * sn + my * c) * scale;
}

// Strike scheduling and the flash (slweather.h, LIGHTNING).
void SLWeather::stepLightning(const Target& t, F32 dt, F64 syncNow, F64 now)
{
    // The storm's build-up: the gate that holds strikes back while it eases in.
    const F32 stormT = t.active ? std::clamp(t.storm, 0.f, 1.f) : 0.f;
    mStormEase += (stormT - mStormEase) * (dt <= 0.f ? 0.f : 1.f - std::exp(-dt / kStormTau));
    if (std::fabs(stormT - mStormEase) < 0.01f) mStormEase = stormT;   // (settled: every strike passes)

    // Slots up to kLookahead ahead. (Re)start at the present -- never replay the past -- after
    // a start, a stall or the clock stepping.
    const S64 last = (S64)std::floor((syncNow + kLookahead) / kSlot);
    const S64 cur = (S64)std::floor(syncNow / kSlot);
    if (!mSlotValid || mNextSlot < cur - 2 || mNextSlot > last + 1)
    {
        mNextSlot = cur;
        mSlotValid = true;
    }
    const F32 rate = strikeRate(t);
    for (; mNextSlot <= last; ++mNextSlot)
    {
        if (rate <= 0.f) continue;
        const S64 slot = mNextSlot;
        const U32 seed = t.seed;
        if (u01(hash3(seed, SALT_STRIKE, slot)) >= rate * (F32)kSlot) continue;
        if (stormT > 0.f && u01(hash3(seed, SALT_GATE, slot)) * stormT > mStormEase) continue;
        Strike s;
        s.serial = ++mStrikeSerial;
        s.syncT0 = (F64)slot * kSlot + (F64)u01(hash3(seed, SALT_TIME, slot)) * kSlot;
        s.t0 = now + (s.syncT0 - syncNow);
        // Distance: nearer in a heavier storm (d = min + range x u^q, q 0.7 .. 2).
        const F32 q = 0.7f + 1.3f * stormT;
        s.dist = kMinDist + (kMaxDist - kMinDist) * std::pow(u01(hash3(seed, SALT_DIST, slot)), q);
        // Azimuth (clockwise from north): around the upwind side (storms come in with the wind),
        // spreading to every side as it gets close (the cell is overhead).
        const U64 ha = hash3(seed, SALT_AZ, slot);
        const F32 tri = u01(ha) + u01(mix64(ha)) + u01(mix64(ha + 1)) - 1.5f;   // -1.5..1.5, bell-shaped
        const F32 spread = 180.f - 100.f * smooth01(800.f, 4000.f, s.dist);
        const F32 az = (t.windDirDeg + tri / 1.5f * spread) * kWeatherRadians;
        // Elevation: the middle of a 1.5..3.5 km channel.
        const F32 h = 1500.f + 2000.f * u01(hash3(seed, SALT_ELEV, slot));
        const F32 el = std::atan2(0.5f * h, s.dist);
        s.dir[0] = std::cos(el) * std::sin(az);
        s.dir[1] = std::cos(el) * std::cos(az);
        s.dir[2] = std::sin(el);
        s.duration = 0.2f + 0.4f * u01(hash3(seed, SALT_SHAPE, slot));
        s.hash = (U32)(hash3(seed, SALT_BITS, slot) >> 32);
        mStrikes.push_back(s);
    }
    // Forget old strikes (bounded either way).
    while (!mStrikes.empty() && (now - mStrikes.front().t0 > kStrikeKeep || mStrikes.size() > kMaxStrikes))
        mStrikes.erase(mStrikes.begin());

    // The brightest flash right now.
    F32 best = 0.f;
    for (const Strike& s : mStrikes)
    {
        const F32 f = flashAt(s, (F32)(now - s.t0));
        if (f <= best) continue;
        best = f;
        mState.lightningDir[0] = s.dir[0];
        mState.lightningDir[1] = s.dir[1];
        mState.lightningDir[2] = s.dir[2];
        mState.lightningDist = s.dist;
    }
    mState.lightning = best;
}
