/**
 * @file alcineliveproberefresh.h
 * @brief [LiveProbeRefresh] Pure scheduler + change-signature model for the
 *        Cine Light Rig Live Probe refresh modes.
 *
 * Header-only and free of GL / viewer globals so it is unit-testable. The
 * reflection manager owns one State, feeds it a quantised input hash H each
 * frame, and captures the cube faces the scheduler asks for. See
 * doc/LIVE_PROBE_REFRESH_DESIGN.md (R3) for the derivation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_CINE_LIVE_PROBE_REFRESH_H
#define AL_CINE_LIVE_PROBE_REFRESH_H

#include "stdtypes.h"
#include "lluuid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ALCineLiveProbeRefresh
{

enum class Mode : S32
{
    EVERY_FRAME = 0,
    BALANCED = 1,
    ECONOMY = 2,
    ON_CHANGE = 3
};

// Out of range -> ON_CHANGE (the shipping default).
inline Mode sanitizeMode(S32 v)
{
    if (v < 0 || v > 3)
    {
        return Mode::ON_CHANGE;
    }
    return static_cast<Mode>(v);
}

enum class Reason : U8
{
    NONE,
    WARMUP,
    CONTINUOUS,
    CHANGED,
    WATCHDOG,
    MANUAL,
    ANIMATED,
    SETTLING,
    CONVERGED
};

inline const char* reasonName(Reason r)
{
    switch (r)
    {
        case Reason::NONE:       return "none";
        case Reason::WARMUP:     return "warm-up";
        case Reason::CONTINUOUS: return "continuous";
        case Reason::CHANGED:    return "changed";
        case Reason::WATCHDOG:   return "watchdog";
        case Reason::MANUAL:     return "manual";
        case Reason::ANIMATED:   return "animated";
        case Reason::SETTLING:   return "settling";
        case Reason::CONVERGED:  return "converged";
    }
    return "none";
}

enum class Path : U8
{
    FULL,
    BUDGET,
    IDLE
};

// ---------------------------------------------------------------------------
// Signature: per-field sticky quantiser -> one 64-bit hash H.
// ---------------------------------------------------------------------------
enum class Tol : U8
{
    ABS,   // scalar:   |v - acc| > a
    REL,   // scalar:   |v - acc| > max(b, a * max(|v|,|acc|))
    ANGLE, // 3-vector: angle(v, acc) > a degrees (near-zero vectors: ABS 1e-6 per axis)
    COLOR  // 3-vector: any channel |v_c - acc_c| > max(b, a * max(|v_c|,|acc_c|))
};

struct Field
{
    Tol mTol;
    U8  mCount; // 1 (ABS/REL) or 3 (ANGLE/COLOR)
    F32 mA;
    F32 mB;
};

namespace detail
{
constexpr U64 kFnvOffset = 1469598103934665603ULL;
constexpr U64 kFnvPrime = 1099511628211ULL;

inline U64 fnvMixByte(U64 h, U8 b)
{
    h ^= static_cast<U64>(b);
    h *= kFnvPrime;
    return h;
}

inline U64 fnvMixU32(U64 h, U32 v)
{
    for (S32 i = 0; i < 4; ++i)
    {
        h = fnvMixByte(h, static_cast<U8>((v >> (8 * i)) & 0xffu));
    }
    return h;
}

inline U64 fnvMixU64(U64 h, U64 v)
{
    for (S32 i = 0; i < 8; ++i)
    {
        h = fnvMixByte(h, static_cast<U8>((v >> (8 * i)) & 0xffu));
    }
    return h;
}

inline U32 floatBits(F32 v)
{
    U32 bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

// Non-finite inputs would wedge the sticky comparison (NaN never fires), so
// they are stored as 0.
inline F32 finiteOrZero(F32 v)
{
    return std::isfinite(v) ? v : 0.f;
}

inline bool scalarFires(const Field& f, F32 v, F32 acc)
{
    const F32 diff = std::fabs(v - acc);
    if (f.mTol == Tol::REL)
    {
        const F32 mag = std::max(std::fabs(v), std::fabs(acc));
        return diff > std::max(f.mB, f.mA * mag);
    }
    return diff > f.mA;
}

inline bool angleFires(const Field& f, const F32* v, const F32* acc)
{
    const F64 vx = static_cast<F64>(v[0]);
    const F64 vy = static_cast<F64>(v[1]);
    const F64 vz = static_cast<F64>(v[2]);
    const F64 ax = static_cast<F64>(acc[0]);
    const F64 ay = static_cast<F64>(acc[1]);
    const F64 az = static_cast<F64>(acc[2]);
    const F64 vlen = std::sqrt(vx * vx + vy * vy + vz * vz);
    const F64 alen = std::sqrt(ax * ax + ay * ay + az * az);
    if (vlen < 1e-6 || alen < 1e-6)
    {
        return std::fabs(vx - ax) > 1e-6 || std::fabs(vy - ay) > 1e-6 ||
               std::fabs(vz - az) > 1e-6;
    }
    const F64 cx = vy * az - vz * ay;
    const F64 cy = vz * ax - vx * az;
    const F64 cz = vx * ay - vy * ax;
    const F64 cross_len = std::sqrt(cx * cx + cy * cy + cz * cz);
    const F64 dot = vx * ax + vy * ay + vz * az;
    const F64 rad_to_deg = 57.295779513082320876798154814105;
    const F64 degrees = std::atan2(cross_len, dot) * rad_to_deg;
    return degrees > static_cast<F64>(f.mA);
}

inline bool colorFires(const Field& f, const F32* v, const F32* acc)
{
    for (S32 c = 0; c < 3; ++c)
    {
        const F32 diff = std::fabs(v[c] - acc[c]);
        const F32 mag = std::max(std::fabs(v[c]), std::fabs(acc[c]));
        if (diff > std::max(f.mB, f.mA * mag))
        {
            return true;
        }
    }
    return false;
}

inline bool fieldFires(const Field& f, const F32* v, const F32* acc)
{
    switch (f.mTol)
    {
        case Tol::ABS:
        case Tol::REL:
            return scalarFires(f, v[0], acc[0]);
        case Tol::ANGLE:
            return angleFires(f, v, acc);
        case Tol::COLOR:
            return colorFires(f, v, acc);
    }
    return false;
}

inline bool sameLayout(const Field& a, const Field& b)
{
    return a.mTol == b.mTol && a.mCount == b.mCount &&
           a.mA == b.mA && a.mB == b.mB;
}
} // namespace detail

struct Signature
{
    // mVal holds sum(mCount) floats in field order.
    std::vector<F32> mVal;
    std::vector<Field> mField;
    // FNV-1a over exact items.
    U64 mExact = detail::kFnvOffset;

    inline void clear()
    {
        mVal.clear();
        mField.clear();
        mExact = detail::kFnvOffset;
    }

    inline void addAbs(F32 v, F32 tol)
    {
        mVal.push_back(detail::finiteOrZero(v));
        mField.push_back(Field{ Tol::ABS, static_cast<U8>(1), tol, 0.f });
    }

    inline void addRel(F32 v, F32 rel, F32 floor_value)
    {
        mVal.push_back(detail::finiteOrZero(v));
        mField.push_back(Field{ Tol::REL, static_cast<U8>(1), rel, floor_value });
    }

    inline void addAngle(const F32* v3, F32 deg)
    {
        for (S32 i = 0; i < 3; ++i)
        {
            mVal.push_back(detail::finiteOrZero(v3[i]));
        }
        mField.push_back(Field{ Tol::ANGLE, static_cast<U8>(3), deg, 0.f });
    }

    inline void addColor(const F32* v3, F32 rel, F32 floor_value)
    {
        for (S32 i = 0; i < 3; ++i)
        {
            mVal.push_back(detail::finiteOrZero(v3[i]));
        }
        mField.push_back(Field{ Tol::COLOR, static_cast<U8>(3), rel, floor_value });
    }

    // Three independent ABS fields (positions, scales, wave directions).
    inline void addAbs3(const F32* v3, F32 tol)
    {
        for (S32 i = 0; i < 3; ++i)
        {
            addAbs(v3[i], tol);
        }
    }

    inline void addExact(U64 bits)
    {
        mExact = detail::fnvMixU64(mExact, bits);
    }

    inline void addExact(const LLUUID& id)
    {
        for (S32 i = 0; i < UUID_BYTES; ++i)
        {
            mExact = detail::fnvMixByte(mExact, id.mData[i]);
        }
    }

    inline void addExact(bool b)
    {
        mExact = detail::fnvMixByte(mExact, static_cast<U8>(b ? 1 : 0));
    }
};

// A field's accepted value(s) move, as a group, only when that field's own
// tolerance test fires. Float noise below tolerance therefore never moves an
// accepted value, and slow drift produces exactly one transition.
struct StickyHash
{
    std::vector<F32> mAcc;
    std::vector<Field> mLayout;

    inline U64 update(const Signature& s)
    {
        bool relayout = mAcc.size() != s.mVal.size() ||
                        mLayout.size() != s.mField.size();
        if (!relayout)
        {
            for (size_t i = 0; i < mLayout.size(); ++i)
            {
                if (!detail::sameLayout(mLayout[i], s.mField[i]))
                {
                    relayout = true;
                    break;
                }
            }
        }
        if (relayout)
        {
            mAcc = s.mVal;
            mLayout = s.mField;
        }
        else
        {
            size_t offset = 0;
            for (size_t i = 0; i < s.mField.size(); ++i)
            {
                const Field& f = s.mField[i];
                if (detail::fieldFires(f, &s.mVal[offset], &mAcc[offset]))
                {
                    for (size_t c = 0; c < static_cast<size_t>(f.mCount); ++c)
                    {
                        mAcc[offset + c] = s.mVal[offset + c];
                    }
                }
                offset += static_cast<size_t>(f.mCount);
            }
        }

        U64 h = detail::kFnvOffset;
        h = detail::fnvMixU64(h, static_cast<U64>(mAcc.size()));
        h = detail::fnvMixU64(h, static_cast<U64>(mLayout.size()));
        for (size_t i = 0; i < mLayout.size(); ++i)
        {
            h = detail::fnvMixByte(h, static_cast<U8>(mLayout[i].mTol));
            h = detail::fnvMixByte(h, mLayout[i].mCount);
            h = detail::fnvMixU32(h, detail::floatBits(mLayout[i].mA));
            h = detail::fnvMixU32(h, detail::floatBits(mLayout[i].mB));
        }
        for (size_t i = 0; i < mAcc.size(); ++i)
        {
            h = detail::fnvMixU32(h, detail::floatBits(mAcc[i]));
        }
        h = detail::fnvMixU64(h, s.mExact);
        return h;
    }
};

// ---------------------------------------------------------------------------
// Scheduler
// ---------------------------------------------------------------------------
struct Params
{
    Mode mMode = Mode::ON_CHANGE;
    S32 mChangeFaces = 2;
    F32 mWatchdogSec = 5.f;
    F32 mSettleSec = 0.5f;
};

struct State
{
    const void* mProbe = nullptr;
    S32 mCubeIndex = -1;
    Mode mMode = Mode::EVERY_FRAME;
    // pass cursor
    bool mActive = false;
    U8 mFace = 0;
    bool mRadiance = false;
    S32 mFaces = 1;
    U64 mPassH = 0;
    bool mPassClean = true;
    // last pass of each kind
    bool mIrrOk = false;
    U64 mIrrH = 0;
    bool mRadOk = false;
    U64 mRadH = 0;
    // kind of the last completed pass
    bool mLastPassIrr = false;
    F64 mLastConverged = -1.0;
    F64 mLastAnim = -1.0;
    bool mManual = false;
    Reason mReason = Reason::NONE;
    F64 mReasonTime = -1.0;
};

struct Decision
{
    Path mPath = Path::IDLE;
    S32 mFaces = 0;
    Reason mReason = Reason::NONE;
};

enum class PassEnd : U8
{
    NONE,
    IRRADIANCE,
    RADIANCE
};

inline bool converged(const State& s, U64 h)
{
    return s.mIrrOk && s.mRadOk && s.mIrrH == h && s.mRadH == h;
}

inline void reset(State& s)
{
    s = State();
}

namespace detail
{
inline void noteReason(State& s, Reason r, F64 now, bool force_time)
{
    if (force_time || r != s.mReason)
    {
        s.mReasonTime = now;
    }
    s.mReason = r;
}

inline void startPass(State& s, bool radiance, S32 faces, U64 h,
                      Reason r, F64 now)
{
    s.mActive = true;
    s.mFace = 0;
    s.mRadiance = radiance;
    s.mFaces = faces;
    s.mPassH = h;
    s.mPassClean = true;
    noteReason(s, r, now, true);
}

inline Decision budgetDecision(const State& s)
{
    Decision d;
    d.mPath = Path::BUDGET;
    d.mFaces = s.mFaces;
    d.mReason = s.mReason;
    return d;
}
} // namespace detail

inline Decision decide(State& s, const Params& p, U64 h, bool animating,
                       bool ready, F64 now, const void* probe, S32 cube_index)
{
    // 1. Identity / mode change.
    if (probe != s.mProbe || cube_index != s.mCubeIndex || p.mMode != s.mMode)
    {
        reset(s);
        s.mProbe = probe;
        s.mCubeIndex = cube_index;
        s.mMode = p.mMode;
    }

    Decision d;

    // 2. Not ready: full capture warm-up.
    if (!ready)
    {
        s.mActive = false;
        detail::noteReason(s, Reason::WARMUP, now, false);
        d.mPath = Path::FULL;
        d.mFaces = 6;
        d.mReason = Reason::WARMUP;
        return d;
    }

    // 3. Pure face budget, always cycling.
    if (p.mMode == Mode::BALANCED || p.mMode == Mode::ECONOMY)
    {
        if (!s.mActive)
        {
            const S32 faces = (p.mMode == Mode::BALANCED) ? 2 : 1;
            detail::startPass(s, s.mLastPassIrr, faces, h,
                              Reason::CONTINUOUS, now);
        }
        return detail::budgetDecision(s);
    }

    // 4. ON_CHANGE animation: the rig's own animation flags -> full path.
    if (animating)
    {
        s.mLastAnim = now;
    }
    if (animating || (s.mLastAnim >= 0.0 &&
                      now - s.mLastAnim < static_cast<F64>(p.mSettleSec)))
    {
        s.mActive = false;
        const Reason r = animating ? Reason::ANIMATED : Reason::SETTLING;
        detail::noteReason(s, r, now, false);
        d.mPath = Path::FULL;
        d.mFaces = 6;
        d.mReason = r;
        return d;
    }

    // 5. A started pass always completes; changes only make it dirty.
    if (s.mActive)
    {
        return detail::budgetDecision(s);
    }

    // 6. Pick a reason.
    Reason reason = Reason::NONE;
    if (s.mManual)
    {
        s.mManual = false;
        s.mIrrOk = false;
        s.mRadOk = false;
        reason = Reason::MANUAL;
    }
    else if (converged(s, h) && p.mWatchdogSec > 0.f &&
             now - s.mLastConverged >= static_cast<F64>(p.mWatchdogSec))
    {
        s.mIrrOk = false;
        s.mRadOk = false;
        reason = Reason::WATCHDOG;
    }
    else if (!converged(s, h))
    {
        if ((s.mReason == Reason::WATCHDOG || s.mReason == Reason::MANUAL) &&
            s.mPassClean && s.mPassH == h)
        {
            // [LiveProbeRefresh] Continuation of a watchdog / manual request
            // (its second pass, or a repeat after a manual press landed right
            // after an irradiance pass): the last pass ran on this same H and
            // stayed clean, so nothing changed since the request. Keep the
            // reason (and its timestamp) so the status does not say "changed".
            const F64 reason_time = s.mReasonTime;
            detail::startPass(s, s.mLastPassIrr, std::clamp(p.mChangeFaces, 1, 6),
                              h, s.mReason, now);
            s.mReasonTime = reason_time;
            return detail::budgetDecision(s);
        }
        reason = (s.mLastConverged < 0.0) ? Reason::WARMUP : Reason::CHANGED;
    }
    else
    {
        d.mPath = Path::IDLE;
        d.mFaces = 0;
        d.mReason = Reason::CONVERGED;
        return d;
    }

    // The pass kind ALWAYS alternates, converged or not.
    const S32 faces = std::clamp(p.mChangeFaces, 1, 6);
    detail::startPass(s, s.mLastPassIrr, faces, h, reason, now);
    return detail::budgetDecision(s);
}

// Every BUDGET frame (blocked or not): a pass spanning a changed H is dirty.
inline void noteFrameH(State& s, U64 h)
{
    if (s.mActive && h != s.mPassH)
    {
        s.mPassClean = false;
    }
}

// After each captured budget face.
inline PassEnd advanceFace(State& s, F64 now)
{
    s.mFace = static_cast<U8>(s.mFace + 1);
    if (s.mFace < 6)
    {
        return PassEnd::NONE;
    }
    s.mActive = false;
    const bool ok = s.mPassClean;
    if (!s.mRadiance)
    {
        s.mIrrOk = ok;
        s.mIrrH = s.mPassH;
        s.mRadOk = false;
        s.mLastPassIrr = true;
        return PassEnd::IRRADIANCE;
    }
    s.mRadOk = ok && s.mIrrOk && s.mIrrH == s.mPassH;
    s.mRadH = s.mPassH;
    s.mLastPassIrr = false;
    if (s.mRadOk)
    {
        s.mLastConverged = now;
    }
    return PassEnd::RADIANCE;
}

// After each FULL frame: one complete pass sampled at one instant is clean.
inline void onFullPass(State& s, bool radiance, U64 h, F64 now)
{
    if (!radiance)
    {
        s.mIrrOk = true;
        s.mIrrH = h;
        s.mRadOk = false;
        s.mLastPassIrr = true;
        return;
    }
    s.mRadOk = s.mIrrOk && s.mIrrH == h;
    s.mRadH = h;
    s.mLastPassIrr = false;
    if (s.mRadOk)
    {
        s.mLastConverged = now;
    }
}

} // namespace ALCineLiveProbeRefresh

#endif // AL_CINE_LIVE_PROBE_REFRESH_H
