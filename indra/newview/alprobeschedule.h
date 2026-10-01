/**
 * @file alprobeschedule.h
 * @brief [ProbeOnDemand] Pure scheduling / dirtiness model for on-demand
 *        reflection probe updates.
 *
 * Header-only and free of GL / viewer globals so it is unit-testable. The
 * reflection manager (llreflectionmapmanager.cpp) and the dirty-event recorder
 * (alprobedirty.cpp) own the viewer-facing glue; every decision that can be
 * expressed on plain numbers lives here. See doc/PROBE_UPDATE_ON_DEMAND_BRIEF_V6.md.
 *
 * Reuses ALCineLiveProbeRefresh::Signature / StickyHash (alcineliveproberefresh.h
 * is included, never edited).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_PROBE_SCHEDULE_H
#define AL_PROBE_SCHEDULE_H

#include "stdtypes.h"
#include "lluuid.h"
#include "alcineliveproberefresh.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ALProbeSched
{

// ---------------------------------------------------------------------------
// Basic vocabulary
// ---------------------------------------------------------------------------
enum Reason : U16
{
    R_GEOM = 1 << 0,
    R_TEX = 1 << 1,
    R_LIGHT = 1 << 2,
    R_ENV = 1 << 3,
    R_PROBE = 1 << 4,
    R_SAFETY = 1 << 5,
    R_CLOUD = 1 << 6,
    R_RESYNC = 1 << 7,
    R_WARMUP = 1 << 8,
    R_DYN = 1 << 9,
    R_SETTLE = 1 << 10,
    R_BARRIER = 1 << 11
};

enum Class : U8
{
    C_STATIC = 1 << 0,
    C_TERRAIN_WATER = 1 << 1,
    C_LIGHT = 1 << 2,
    C_GLOBAL = 1 << 3
};

enum class Motion : U8
{
    NONE,
    XFORM,
    TEXANIM,
    LIGHT_XFORM,
    LIGHT_PHOTO
};

// Letters of a reason mask for the debug text / log:
// G T L E P S C R W D Z B (geom, tex, light, env, probe, safety, cloud, resync,
// warm-up, dynamic, settle, barrier).
inline std::string reasonLetters(U16 reasons)
{
    static const char kLetters[] = { 'G', 'T', 'L', 'E', 'P', 'S', 'C', 'R', 'W', 'D', 'Z', 'B' };
    std::string out;
    for (S32 i = 0; i < 12; ++i)
    {
        if (reasons & static_cast<U16>(1u << i))
        {
            out.push_back(kLetters[i]);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Small geometry helpers on plain arrays (no LLVector dependency)
// ---------------------------------------------------------------------------
inline void boxEmpty(F32 mn[3], F32 mx[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        mn[i] = 1.0e30f;
        mx[i] = -1.0e30f;
    }
}

inline bool boxIsEmpty(const F32 mn[3], const F32 mx[3])
{
    return mn[0] > mx[0] || mn[1] > mx[1] || mn[2] > mx[2];
}

inline void boxUnion(F32 mn[3], F32 mx[3], const F32 omn[3], const F32 omx[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        mn[i] = std::min(mn[i], omn[i]);
        mx[i] = std::max(mx[i], omx[i]);
    }
}

inline void boxSphere(F32 mn[3], F32 mx[3], const F32 c[3], F32 r)
{
    for (S32 i = 0; i < 3; ++i)
    {
        mn[i] = c[i] - r;
        mx[i] = c[i] + r;
    }
}

inline void boxOffset(F32 mn[3], F32 mx[3], const F32 off[3])
{
    if (boxIsEmpty(mn, mx))
    {
        return;
    }
    for (S32 i = 0; i < 3; ++i)
    {
        mn[i] += off[i];
        mx[i] += off[i];
    }
}

inline bool boxFinite(const F32 mn[3], const F32 mx[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        if (!std::isfinite(mn[i]) || !std::isfinite(mx[i]))
        {
            return false;
        }
    }
    return true;
}

// Squared distance from point p to the box.
inline F32 boxDist2(const F32 mn[3], const F32 mx[3], const F32 p[3])
{
    F32 d2 = 0.f;
    for (S32 i = 0; i < 3; ++i)
    {
        const F32 lo = mn[i] - p[i];
        const F32 hi = p[i] - mx[i];
        const F32 d = std::max(std::max(lo, hi), 0.f);
        d2 += d * d;
    }
    return d2;
}

inline bool aabbVsSphere(const F32 mn[3], const F32 mx[3], const F32 o[3], F32 r)
{
    return boxDist2(mn, mx, o) <= r * r;
}

// Bounding sphere (centre, radius) of a box.
inline void boxBoundingSphere(const F32 mn[3], const F32 mx[3], F32 c[3], F32& r)
{
    F32 h2 = 0.f;
    for (S32 i = 0; i < 3; ++i)
    {
        c[i] = 0.5f * (mn[i] + mx[i]);
        const F32 h = 0.5f * (mx[i] - mn[i]);
        h2 += h * h;
    }
    r = std::sqrt(h2);
}

// Squared distance from p to the segment a->b.
inline F32 pointSegDist2(const F32 p[3], const F32 a[3], const F32 b[3])
{
    F32 ab[3], ap[3];
    F32 ab2 = 0.f, t = 0.f;
    for (S32 i = 0; i < 3; ++i)
    {
        ab[i] = b[i] - a[i];
        ap[i] = p[i] - a[i];
        ab2 += ab[i] * ab[i];
        t += ap[i] * ab[i];
    }
    t = (ab2 > 1e-12f) ? std::clamp(t / ab2, 0.f, 1.f) : 0.f;
    F32 d2 = 0.f;
    for (S32 i = 0; i < 3; ++i)
    {
        const F32 d = ap[i] - t * ab[i];
        d2 += d * d;
    }
    return d2;
}

inline U64 idTagOf(const LLUUID& id)
{
    return id.getDigest64();
}

// ---------------------------------------------------------------------------
// Event / Record / Policy / View
// ---------------------------------------------------------------------------
struct Event
{
    F32 mMin[3] = { 0.f, 0.f, 0.f };
    F32 mMax[3] = { 0.f, 0.f, 0.f };
    U16 mReason = 0;
    U8 mClass = 0;
    // 0 = unconditional; else hit only probes with mLastFaceOp > mMinFaceOp
    // (the texture was blurred at that op; probes that rendered earlier never
    // saw the blurred version).
    U64 mMinFaceOp = 0;
    // Flush serial when the motion behind this event began (current serial for
    // a discrete event). Drives the over-cap history rule.
    U64 mFirstSerial = 0;
    // Light event of a spot light (spots are candidates at any distance).
    bool mSpot = false;
};

inline Event makeEvent(const F32 mn[3], const F32 mx[3], U16 reason, U8 cls,
                       U64 first_serial)
{
    Event e;
    for (S32 i = 0; i < 3; ++i)
    {
        e.mMin[i] = mn[i];
        e.mMax[i] = mx[i];
    }
    e.mReason = reason;
    e.mClass = cls;
    e.mFirstSerial = first_serial;
    return e;
}

enum class Owner : U8
{
    ORDINARY,
    LIVE,
    REALTIME
};

struct Record
{
    U32 mId = 0; // monotonic, never reused
    U64 mDirtySerial = 0;
    U64 mAckSerial = 0;
    U64 mTxnSerial = 0;
    U64 mLastFaceOp = 0;
    U32 mTxnEpoch = 0;
    bool mInTxn = false;
    bool mTxnIrrDone = false;
    S32 mTxnCube = -1;
    U16 mPending = 0;
    U16 mTxnReasons = 0;
    U16 mLastReasons = 0;
    F64 mFirstDirty = -1.0;
    F64 mFirstDirtyInTxn = -1.0;
    F64 mLastStart = -1.0;
    F64 mLastComplete = -1.0;
    U32 mStartStreakWindows = 0; // UNSETTLED (see noteStartWindow)
    U32 mLastStartWindow = 0;
    U16 mLocalLights = 0; // eligible lights that can reach this probe (cap rule)
    U16 mLocalLightsPrev = 0;
    U64 mOverCapSerial = 0;
    bool mRecountDue = true;
    U32 mRecountDirtyEpoch = 0; // bumped on every re-dirty of the recount
    U64 mLastRecountSerial = 0; // age-based turn for the recount scheduler
    // State captured at the last transaction start (R_PROBE detection).
    bool mHasStart = false;
    F32 mStartOrigin[3] = { 0.f, 0.f, 0.f };
    F32 mStartRadius = 0.f;
    F32 mStartAmbiance = 0.f;
    F32 mStartNear = 0.f;
    bool mStartDynamic = false;
    bool mStartBox = false;
    S32 mStartCube = -1;
    F32 mStartCam[3] = { 0.f, 0.f, 0.f };  // default probe only
    F32 mStartCloud[2] = { 0.f, 0.f };     // default probe only
    Owner mOwner = Owner::ORDINARY;
};

struct Policy
{
    F32 mMinInterval = 1.f;
    F32 mMaxAge = 60.f;
};

struct View
{
    bool mComplete = false;
    bool mOccluded = false;
    bool mDynamic = false;
    bool mAllocated = false;
    bool mBarrier = false;
};

enum class Why : U8
{
    NONE,
    WARMUP,
    BARRIER,
    DIRTY,
    SAFETY,
    DYNAMIC,
    WAIT_INTERVAL,
    DEFERRED
};

inline bool dirty(const Record& r)
{
    return r.mDirtySerial > r.mAckSerial;
}

inline bool eligible(Why w)
{
    return w == Why::WARMUP || w == Why::BARRIER || w == Why::DIRTY ||
           w == Why::DYNAMIC || w == Why::SAFETY;
}

// Reason bit a selection verdict contributes to the transaction's reasons.
inline U16 whyReason(Why w)
{
    switch (w)
    {
        case Why::WARMUP:  return R_WARMUP;
        case Why::BARRIER: return R_BARRIER;
        case Why::SAFETY:  return R_SAFETY;
        case Why::DYNAMIC: return R_DYN;
        default:           return 0;
    }
}

inline void hit(Record& r, U16 reason, U64 serial, F64 now)
{
    r.mDirtySerial = std::max(r.mDirtySerial, serial);
    r.mPending = static_cast<U16>(r.mPending | reason);
    if (r.mInTxn)
    {
        if (r.mFirstDirtyInTxn < 0.0)
        {
            r.mFirstDirtyInTxn = now;
        }
    }
    else if (r.mFirstDirty < 0.0)
    {
        r.mFirstDirty = now;
    }
}

// What the capture looked like when the transaction started (R_PROBE source).
struct CaptureState
{
    F32 mOrigin[3] = { 0.f, 0.f, 0.f };
    F32 mRadius = 0.f;
    F32 mAmbiance = 0.f;
    F32 mNear = 0.f;
    bool mDynamic = false;
    bool mBox = false;
    F32 mCam[3] = { 0.f, 0.f, 0.f };
    F32 mCloud[2] = { 0.f, 0.f };
};

inline void clearTxn(Record& r)
{
    r.mInTxn = false;
    r.mTxnIrrDone = false;
    r.mTxnCube = -1;
    r.mFirstDirtyInTxn = -1.0;
}

inline void onTxnStart(Record& r, U64 serial, U32 epoch, F64 now, U16 reasons,
                       S32 cube, const CaptureState* cap = nullptr)
{
    r.mInTxn = true;
    r.mTxnIrrDone = false;
    r.mTxnSerial = serial;
    r.mTxnEpoch = epoch;
    r.mTxnCube = cube;
    r.mLastStart = now;
    r.mTxnReasons = reasons;
    r.mFirstDirtyInTxn = -1.0;
    r.mPending = 0; // everything pending so far is covered by this transaction
    r.mStartCube = cube;
    if (cap)
    {
        r.mHasStart = true;
        for (S32 i = 0; i < 3; ++i)
        {
            r.mStartOrigin[i] = cap->mOrigin[i];
            r.mStartCam[i] = cap->mCam[i];
        }
        r.mStartRadius = cap->mRadius;
        r.mStartAmbiance = cap->mAmbiance;
        r.mStartNear = cap->mNear;
        r.mStartDynamic = cap->mDynamic;
        r.mStartBox = cap->mBox;
        r.mStartCloud[0] = cap->mCloud[0];
        r.mStartCloud[1] = cap->mCloud[1];
    }
}

inline void onTxnIrradianceDone(Record& r, U32 epoch)
{
    if (r.mInTxn && epoch == r.mTxnEpoch)
    {
        r.mTxnIrrDone = true;
    }
}

// True = acked (the capture is a complete irradiance + radiance pair of this
// transaction). False = nack: the transaction is dropped and an R_RESYNC hit
// at the current frame serial keeps the probe dirty. `serial` is the current
// flush serial (it never exceeds the serial of the next transaction start, so
// the redo's ack always covers the resync).
inline bool onTxnComplete(Record& r, U32 epoch, S32 cube, F64 now, U64 serial)
{
    const bool ok = r.mInTxn && r.mTxnIrrDone && epoch == r.mTxnEpoch &&
                    cube == r.mTxnCube;
    if (ok)
    {
        r.mAckSerial = std::max(r.mAckSerial, r.mTxnSerial);
        r.mLastComplete = now;
        r.mLastReasons = r.mTxnReasons;
        if (!dirty(r))
        {
            r.mPending = 0;
            r.mFirstDirty = -1.0;
        }
        else
        {
            r.mFirstDirty = r.mFirstDirtyInTxn;
        }
        clearTxn(r);
        return true;
    }
    const U16 restore = r.mInTxn ? r.mTxnReasons : static_cast<U16>(0);
    if (r.mFirstDirty < 0.0 && r.mFirstDirtyInTxn >= 0.0)
    {
        r.mFirstDirty = r.mFirstDirtyInTxn;
    }
    clearTxn(r);
    r.mPending = static_cast<U16>(r.mPending | restore);
    // Keep the serial strictly newer than the last ack so the probe stays dirty.
    hit(r, R_RESYNC, std::max(serial, r.mAckSerial + 1), now);
    return false;
}

inline bool safetyDue(const Record& r, const Policy& p, F64 now)
{
    return r.mLastComplete >= 0.0 &&
           now - r.mLastComplete >= static_cast<F64>(p.mMaxAge);
}

inline Why evaluate(const Record& r, const View& v, const Policy& p, F64 now)
{
    if (!v.mAllocated)
    {
        return Why::NONE;
    }
    if (!v.mComplete)
    {
        return Why::WARMUP;
    }
    if (v.mBarrier)
    {
        return Why::BARRIER;
    }
    const bool is_dirty = dirty(r);
    const bool safety = safetyDue(r, p, now);
    if (v.mOccluded)
    {
        return (is_dirty || safety) ? Why::DEFERRED : Why::NONE;
    }
    if (r.mLastStart >= 0.0 &&
        now - r.mLastStart < static_cast<F64>(p.mMinInterval))
    {
        return (is_dirty || v.mDynamic || safety) ? Why::WAIT_INTERVAL : Why::NONE;
    }
    if (is_dirty)
    {
        return Why::DIRTY;
    }
    if (v.mDynamic)
    {
        return Why::DYNAMIC;
    }
    if (safety)
    {
        return Why::SAFETY;
    }
    return Why::NONE;
}

// Only ORDINARY-owned records are ever selected for an ordinary transaction.
inline bool selectable(const Record& r, const View& v, const Policy& p, F64 now)
{
    return r.mOwner == Owner::ORDINARY && eligible(evaluate(r, v, p, now));
}

// A complete probe that has never completed a transaction in this schedule
// run gets a full MaxAge of grace instead of an immediate safety refresh.
inline void graceInit(Record& r, bool complete, F64 now)
{
    if (complete && r.mLastComplete < 0.0)
    {
        r.mLastComplete = now;
    }
}

// R_PROBE: did the probe's own capture inputs move since the transaction
// start? origin > 0.1 m, radius > 0.1 m, ambiance > 0.01, near clip > 0.01 m,
// a dynamic or box/sphere flip, or a different cube slot.
inline bool probeChanged(const Record& r, const CaptureState& cur, S32 cube)
{
    if (!r.mHasStart)
    {
        return false;
    }
    for (S32 i = 0; i < 3; ++i)
    {
        if (std::fabs(cur.mOrigin[i] - r.mStartOrigin[i]) > 0.1f)
        {
            return true;
        }
    }
    return std::fabs(cur.mRadius - r.mStartRadius) > 0.1f ||
           std::fabs(cur.mAmbiance - r.mStartAmbiance) > 0.01f ||
           std::fabs(cur.mNear - r.mStartNear) > 0.01f ||
           cur.mDynamic != r.mStartDynamic || cur.mBox != r.mStartBox ||
           cube != r.mStartCube;
}

// Default probe reasons: the CAMERA moved > 16 m since the transaction start
// (never compared with the +64 m capture origin), or the cloud scroll moved
// and a full update period elapsed.
inline U16 defaultProbeReasons(const Record& r, const F32 cam[3], const F32 cloud[2],
                               F64 now, F32 period)
{
    if (!r.mHasStart)
    {
        return 0;
    }
    U16 out = 0;
    F32 d2 = 0.f;
    for (S32 i = 0; i < 3; ++i)
    {
        const F32 d = cam[i] - r.mStartCam[i];
        d2 += d * d;
    }
    if (d2 > 16.f * 16.f)
    {
        out = static_cast<U16>(out | R_PROBE);
    }
    const bool cloud_moved = std::fabs(cloud[0] - r.mStartCloud[0]) > 1e-4f ||
                             std::fabs(cloud[1] - r.mStartCloud[1]) > 1e-4f;
    if (cloud_moved && now - r.mLastStart >= static_cast<F64>(period))
    {
        out = static_cast<U16>(out | R_CLOUD);
    }
    return out;
}

// UNSETTLED bookkeeping: a transaction start with reasons outside S/W/R/B in
// 3 consecutive windows is a streak. Returns true once the streak reaches 3.
inline bool noteStartWindow(Record& r, U32 window, U16 reasons)
{
    const U16 benign = static_cast<U16>(R_SAFETY | R_WARMUP | R_RESYNC | R_BARRIER);
    const bool offending = (reasons & static_cast<U16>(~benign)) != 0;
    if (r.mLastStartWindow == window)
    {
        if (offending && r.mStartStreakWindows == 0)
        {
            r.mStartStreakWindows = 1;
        }
    }
    else if (offending)
    {
        r.mStartStreakWindows = (r.mLastStartWindow + 1 == window)
            ? r.mStartStreakWindows + 1 : 1;
    }
    else
    {
        r.mStartStreakWindows = 0;
    }
    r.mLastStartWindow = window;
    return r.mStartStreakWindows >= 3;
}

// ---------------------------------------------------------------------------
// Footprint: does an event reach a probe's capture?
// ---------------------------------------------------------------------------
struct ShadowDirs
{
    bool mEnabled = false;      // RenderShadowDetail > 0
    bool mSunValid = false;
    bool mMoonValid = false;
    F32 mSun[3] = { 0.f, 0.f, 1.f };   // direction TO the sun (shadows fall along -sun)
    F32 mMoon[3] = { 0.f, 0.f, 1.f };
};

constexpr F32 kShadowCastLength = 256.f;

// Does the 256 m capsule from the event's bounding sphere along `-dir` reach
// the probe's capture sphere?
inline bool capsuleReaches(const F32 c[3], F32 r, const F32 dir[3],
                           const F32 o[3], F32 rcap)
{
    F32 b[3];
    for (S32 i = 0; i < 3; ++i)
    {
        b[i] = c[i] - dir[i] * kShadowCastLength;
    }
    const F32 reach = r + rcap;
    return pointSegDist2(o, c, b) <= reach * reach;
}

// A light event is unconditionally relevant to a probe whose local light count
// is (or was, since the motion began) over the RenderLocalLightCount cap: the
// distance-ordered nearest-N list of such a probe can change for a light that
// is nowhere near it.
inline bool lightUnconditional(const Event& ev, const Record& rec, S32 cap)
{
    const S32 cur = static_cast<S32>(rec.mLocalLights);
    const S32 prev = static_cast<S32>(rec.mLocalLightsPrev);
    return prev > cap || cur > cap || rec.mRecountDue ||
           (rec.mOverCapSerial != 0 && rec.mOverCapSerial >= ev.mFirstSerial);
}

// `mask`: classes this probe renders (ordinary C_STATIC|C_TERRAIN_WATER|C_LIGHT|
// C_GLOBAL; default probe C_TERRAIN_WATER|C_LIGHT|C_GLOBAL).
inline bool hitsCapture(const Event& ev, const Record& rec, const F32 origin[3],
                        F32 rcap, U8 mask, S32 light_cap, const ShadowDirs& sd)
{
    if (ev.mMinFaceOp != 0 && rec.mLastFaceOp <= ev.mMinFaceOp)
    {
        return false; // the probe never rendered a face after the blur
    }
    const U8 cls = static_cast<U8>(ev.mClass & mask);
    if (cls == 0)
    {
        return false;
    }
    if (cls & (C_GLOBAL | C_TERRAIN_WATER))
    {
        return true;
    }
    F32 c[3];
    F32 r = 0.f;
    boxBoundingSphere(ev.mMin, ev.mMax, c, r);
    if (cls & C_LIGHT)
    {
        if (lightUnconditional(ev, rec, light_cap))
        {
            return true;
        }
        F32 d2 = 0.f;
        for (S32 i = 0; i < 3; ++i)
        {
            const F32 d = c[i] - origin[i];
            d2 += d * d;
        }
        const F32 reach = r + rcap;
        if (d2 <= reach * reach)
        {
            return true;
        }
    }
    if (cls & C_STATIC)
    {
        if (aabbVsSphere(ev.mMin, ev.mMax, origin, rcap))
        {
            return true;
        }
        // Only on a miss, and only with shadows on: the object's shadow can
        // land inside the capture.
        if (sd.mEnabled)
        {
            if (sd.mSunValid && capsuleReaches(c, r, sd.mSun, origin, rcap))
            {
                return true;
            }
            if (sd.mMoonValid && capsuleReaches(c, r, sd.mMoon, origin, rcap))
            {
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// stickyDigest: the tail of StickyHash::update (alcineliveproberefresh.h),
// reproduced bit-for-bit so a stored hash can be recomputed after the
// accumulators were shifted (region crossing) without re-running update().
// ---------------------------------------------------------------------------
inline U64 stickyDigest(const std::vector<F32>& acc,
                        const std::vector<ALCineLiveProbeRefresh::Field>& layout,
                        U64 exact)
{
    namespace det = ALCineLiveProbeRefresh::detail;
    U64 h = det::kFnvOffset;
    h = det::fnvMixU64(h, static_cast<U64>(acc.size()));
    h = det::fnvMixU64(h, static_cast<U64>(layout.size()));
    for (size_t i = 0; i < layout.size(); ++i)
    {
        h = det::fnvMixByte(h, static_cast<U8>(layout[i].mTol));
        h = det::fnvMixByte(h, layout[i].mCount);
        h = det::fnvMixU32(h, det::floatBits(layout[i].mA));
        h = det::fnvMixU32(h, det::floatBits(layout[i].mB));
    }
    for (size_t i = 0; i < acc.size(); ++i)
    {
        h = det::fnvMixU32(h, det::floatBits(acc[i]));
    }
    h = det::fnvMixU64(h, exact);
    return h;
}

inline U64 stickyDigest(const ALCineLiveProbeRefresh::StickyHash& s, U64 exact)
{
    return stickyDigest(s.mAcc, s.mLayout, exact);
}

// ---------------------------------------------------------------------------
// Motion debounce (U2): continuous motion is frozen until it stops, then one
// settle event. Per-(key, motion) adaptive quiet window.
// ---------------------------------------------------------------------------
struct DebounceConfig
{
    F32 mQminFloor = 0.5f;      // seconds
    F32 mQminDtMul = 3.f;       // quiet >= 3 frame times at low fps
    F32 mQmax = 30.f;           // seconds
    F32 mForget = 120.f;        // seconds unseen before a non-pending key is erased
    U32 mMapCap = 16384;
    U32 mEvictPerPass = 1024;
    U32 mSettleMergeCap = 1024; // settles per drain before the rest merge into one
    F32 mBulkDeadline = 10.f;   // seconds, hard bound of the bulk state
    F32 mSameInstant = 1e-4f;   // gaps below this are the same instant (no EMA update)
    F32 mScanBackoff = 0.25f;   // seconds before a failed make-room scan is retried
};

struct Debounce
{
    U64 mIdTag = 0;
    F64 mLast = 0.0;
    F64 mSettleAt = 0.0;
    F64 mPendingSince = 0.0;
    U64 mFirstSerial = 0;
    F32 mGapEma = -1.f;
    U16 mCount = 0; // saturating
    bool mPending = false;
    F32 mMin[3] = { 1.0e30f, 1.0e30f, 1.0e30f };
    F32 mMax[3] = { -1.0e30f, -1.0e30f, -1.0e30f };
    U16 mReasons = 0;
    U8 mClass = 0;
    bool mSpot = false;
};

inline F32 debQmin(const DebounceConfig& c, F32 dt)
{
    return std::max(c.mQminFloor, c.mQminDtMul * dt);
}

// Steps 2-4 of onMotion for an existing (or fresh) state.
inline void debOnMotion(const DebounceConfig& c, Debounce& d, F64 now, F32 dt,
                        const F32 mn[3], const F32 mx[3], U16 reasons, U8 cls,
                        bool spot, U64 serial)
{
    if (d.mCount > 0)
    {
        const F64 gap = now - d.mLast;
        if (gap >= static_cast<F64>(c.mSameInstant))
        {
            if (gap > 2.0 * static_cast<F64>(c.mQmax))
            {
                d.mGapEma = -1.f; // a long-idle key is treated as fresh
            }
            else
            {
                d.mGapEma = (d.mGapEma < 0.f)
                    ? static_cast<F32>(gap)
                    : 0.5f * d.mGapEma + 0.5f * static_cast<F32>(gap);
            }
        }
    }
    d.mCount = static_cast<U16>(std::min<U32>(static_cast<U32>(d.mCount) + 1u, 0xFFFFu));
    d.mLast = now;
    if (!d.mPending)
    {
        d.mPending = true;
        d.mPendingSince = now;
        d.mFirstSerial = serial;
        boxEmpty(d.mMin, d.mMax);
        d.mReasons = 0;
        d.mClass = 0;
        d.mSpot = false;
    }
    boxUnion(d.mMin, d.mMax, mn, mx);
    d.mReasons = static_cast<U16>(d.mReasons | reasons);
    d.mClass = static_cast<U8>(d.mClass | cls);
    d.mSpot = d.mSpot || spot;
    const F32 raw = (d.mGapEma < 0.f) ? 0.f : 2.f * d.mGapEma;
    const F32 q = std::min(std::max(raw, debQmin(c, dt)), c.mQmax);
    d.mSettleAt = now + static_cast<F64>(q);
}

inline bool debDue(const Debounce& d, F64 now)
{
    return d.mPending && now >= d.mSettleAt;
}

// Emit the pending union as a settle event and restart accumulation.
inline Event debTake(Debounce& d)
{
    Event e = makeEvent(d.mMin, d.mMax, static_cast<U16>(d.mReasons | R_SETTLE),
                        d.mClass, d.mFirstSerial);
    e.mSpot = d.mSpot;
    d.mPending = false;
    d.mReasons = 0;
    d.mClass = 0;
    d.mSpot = false;
    boxEmpty(d.mMin, d.mMax);
    return e;
}

// Merge a pending state into an event (teardown / removal).
inline void debMergeInto(const Debounce& d, Event& ev)
{
    if (!d.mPending)
    {
        return;
    }
    boxUnion(ev.mMin, ev.mMax, d.mMin, d.mMax);
    ev.mReason = static_cast<U16>(ev.mReason | d.mReasons);
    ev.mClass = static_cast<U8>(ev.mClass | d.mClass);
    ev.mSpot = ev.mSpot || d.mSpot;
    if (d.mFirstSerial != 0 && (ev.mFirstSerial == 0 || d.mFirstSerial < ev.mFirstSerial))
    {
        ev.mFirstSerial = d.mFirstSerial;
    }
}

class DebounceMap
{
public:
    struct Key
    {
        const void* mPtr = nullptr;
        U8 mMotion = 0;
        bool operator==(const Key& o) const
        {
            return mPtr == o.mPtr && mMotion == o.mMotion;
        }
    };
    struct KeyHash
    {
        size_t operator()(const Key& k) const
        {
            const U64 p = static_cast<U64>(reinterpret_cast<uintptr_t>(k.mPtr));
            return static_cast<size_t>((p * 0x9E3779B97F4A7C15ull) ^
                                       (static_cast<U64>(k.mMotion) << 56));
        }
    };
    struct Stats
    {
        U32 mNotes = 0;
        U32 mSettles = 0;
        U32 mEarlySettles = 0;
        U32 mBulkNotes = 0;
        U32 mBulkDeadlines = 0;
        U32 mEvictions = 0;
        U32 mIdMismatchEmits = 0;
    };

    explicit DebounceMap(const DebounceConfig& c = DebounceConfig()) : mCfg(c) {}

    void onMotion(const void* ptr, U8 motion, U64 id_tag, const F32 mn[3],
                  const F32 mx[3], U16 reasons, U8 cls, bool spot, U64 serial,
                  F64 now, F32 dt)
    {
        ++mStats.mNotes;
        Key key;
        key.mPtr = ptr;
        key.mMotion = motion;
        auto it = mMap.find(key);
        if (it != mMap.end())
        {
            Debounce& d = it->second;
            if (d.mIdTag != id_tag)
            {
                // Pointer reuse: deliver what the old object had pending
                // before the state is reset.
                if (d.mPending)
                {
                    mOut.push_back(debTake(d));
                    --mPendingCount;
                    ++mStats.mIdMismatchEmits;
                }
                d = Debounce();
                d.mIdTag = id_tag;
            }
            const bool was_pending = d.mPending;
            debOnMotion(mCfg, d, now, dt, mn, mx, reasons, cls, spot, serial);
            if (!was_pending && d.mPending)
            {
                ++mPendingCount;
            }
            return;
        }
        if (mMap.size() >= static_cast<size_t>(mCfg.mMapCap) && !makeRoom(now, dt))
        {
            // Every tracked key is actively moving: fold into the bulk state
            // (single union, hard deadline) instead of dropping or overflowing.
            ++mStats.mBulkNotes;
            debOnMotion(mCfg, mBulk, now, dt, mn, mx, reasons, cls, spot, serial);
            return;
        }
        Debounce d;
        d.mIdTag = id_tag;
        debOnMotion(mCfg, d, now, dt, mn, mx, reasons, cls, spot, serial);
        mMap.emplace(key, d);
        ++mPendingCount;
    }

    // Teardown: every state of `ptr` (any motion) is merged into `merged` (when
    // pending) and erased. Returns true when something pending was merged.
    bool takePending(const void* ptr, Event& merged)
    {
        bool any = false;
        for (S32 m = 1; m <= 4; ++m)
        {
            Key key;
            key.mPtr = ptr;
            key.mMotion = static_cast<U8>(m);
            auto it = mMap.find(key);
            if (it == mMap.end())
            {
                continue;
            }
            if (it->second.mPending)
            {
                debMergeInto(it->second, merged);
                --mPendingCount;
                any = true;
            }
            mMap.erase(it);
        }
        return any;
    }

    // Appends everything due: ids emitted since the last drain, settled states,
    // the bulk state at its deadline. Also forgets long-unseen idle keys.
    void drain(F64 now, std::vector<Event>& out)
    {
        for (const Event& e : mOut)
        {
            out.push_back(e);
        }
        mStats.mSettles += static_cast<U32>(mOut.size());
        mOut.clear();

        if (mPendingCount > 0)
        {
            mDue.clear();
            for (auto& kv : mMap)
            {
                if (debDue(kv.second, now))
                {
                    mDue.push_back(debTake(kv.second));
                    --mPendingCount;
                }
            }
            emitDue(out);
        }

        if (mBulk.mPending)
        {
            const bool quiet = now >= mBulk.mSettleAt;
            const bool deadline = now >= mBulk.mPendingSince + static_cast<F64>(mCfg.mBulkDeadline);
            if (quiet || deadline)
            {
                if (!quiet)
                {
                    ++mStats.mBulkDeadlines;
                }
                out.push_back(debTake(mBulk));
                ++mStats.mSettles;
            }
        }

        if (now >= mNextForget)
        {
            mNextForget = now + 1.0;
            for (auto it = mMap.begin(); it != mMap.end();)
            {
                if (!it->second.mPending &&
                    now - it->second.mLast > static_cast<F64>(mCfg.mForget))
                {
                    it = mMap.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    // Region crossing: queued bounds move with the world.
    void shift(const F32 off[3])
    {
        for (auto& kv : mMap)
        {
            boxOffset(kv.second.mMin, kv.second.mMax, off);
        }
        boxOffset(mBulk.mMin, mBulk.mMax, off);
        for (Event& e : mOut)
        {
            boxOffset(e.mMin, e.mMax, off);
        }
    }

    void clear()
    {
        mMap.clear();
        mOut.clear();
        mDue.clear();
        mBulk = Debounce();
        mPendingCount = 0;
        mNoScanUntil = 0.0;
        mNextForget = 0.0;
    }

    size_t size() const { return mMap.size(); }
    U32 pendingCount() const { return mPendingCount + (mBulk.mPending ? 1u : 0u); }
    bool bulkPending() const { return mBulk.mPending; }
    const Stats& stats() const { return mStats; }
    void resetStats() { mStats = Stats(); }
    const DebounceConfig& config() const { return mCfg; }

private:
    // Cap policy (CX4): (1) evict settled history, (2) settle quiet pending
    // keys early, (3) only then the caller folds into bulk.
    bool makeRoom(F64 now, F32 dt)
    {
        if (now < mNoScanUntil)
        {
            return false;
        }
        std::vector<std::pair<F64, Key>> history;
        for (const auto& kv : mMap)
        {
            if (!kv.second.mPending)
            {
                history.emplace_back(kv.second.mLast, kv.first);
            }
        }
        if (!history.empty())
        {
            const size_t n = std::min(history.size(), static_cast<size_t>(mCfg.mEvictPerPass));
            std::partial_sort(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(n),
                              history.end(),
                              [](const std::pair<F64, Key>& a, const std::pair<F64, Key>& b)
                              { return a.first < b.first; });
            for (size_t i = 0; i < n; ++i)
            {
                mMap.erase(history[i].second);
            }
            mStats.mEvictions += static_cast<U32>(n);
            return true;
        }
        const F64 quiet = static_cast<F64>(debQmin(mCfg, dt));
        U32 freed = 0;
        for (auto it = mMap.begin(); it != mMap.end();)
        {
            if (it->second.mPending && now - it->second.mLast >= quiet)
            {
                mOut.push_back(debTake(it->second));
                --mPendingCount;
                ++mStats.mEarlySettles;
                it = mMap.erase(it);
                ++freed;
            }
            else
            {
                ++it;
            }
        }
        if (freed > 0)
        {
            return true;
        }
        mNoScanUntil = now + static_cast<F64>(mCfg.mScanBackoff);
        return false;
    }

    // More than mSettleMergeCap settles in one drain: the rest merge into one
    // union event.
    void emitDue(std::vector<Event>& out)
    {
        mStats.mSettles += static_cast<U32>(mDue.size());
        const size_t cap = std::max<size_t>(1u, static_cast<size_t>(mCfg.mSettleMergeCap));
        if (mDue.size() <= cap)
        {
            for (const Event& e : mDue)
            {
                out.push_back(e);
            }
            return;
        }
        for (size_t i = 0; i + 1 < cap; ++i)
        {
            out.push_back(mDue[i]);
        }
        Event merged = mDue[cap - 1];
        for (size_t i = cap; i < mDue.size(); ++i)
        {
            const Event& e = mDue[i];
            boxUnion(merged.mMin, merged.mMax, e.mMin, e.mMax);
            merged.mReason = static_cast<U16>(merged.mReason | e.mReason);
            merged.mClass = static_cast<U8>(merged.mClass | e.mClass);
            merged.mSpot = merged.mSpot || e.mSpot;
            merged.mFirstSerial = std::min(merged.mFirstSerial, e.mFirstSerial);
        }
        out.push_back(merged);
    }

    DebounceConfig mCfg;
    std::unordered_map<Key, Debounce, KeyHash> mMap;
    std::vector<Event> mOut;  // settles emitted outside drain (id mismatch, early)
    std::vector<Event> mDue;  // drain scratch
    Debounce mBulk;
    U32 mPendingCount = 0;
    F64 mNoScanUntil = 0.0;
    F64 mNextForget = 0.0;
    Stats mStats;
};

// ---------------------------------------------------------------------------
// Probe-centric lights (U1): the camera-independent light diff.
// ---------------------------------------------------------------------------
enum LightSig : S32
{
    LS_XFORM = 0,
    LS_PHOTO = 1,
    LS_STRUCT = 2,
    LS_COUNT = 3
};

struct GoboSample
{
    S32 mPattern = 0;
    S32 mAnimMode = 0;
    F32 mSpeed = 0.f;
    F32 mZoom = 0.f;
    F32 mDispersion = 0.f;
    F32 mTint[3] = { 0.f, 0.f, 0.f };
    F32 mParams[4] = { 0.f, 0.f, 0.f, 0.f };
};

// Everything the capture could use from one light, read by the manager from
// the LLVOVolume (kept free of viewer types so the diff is unit-testable).
struct LightSample
{
    LLUUID mId;
    bool mEligible = false;
    F32 mPos[3] = { 0.f, 0.f, 0.f };
    F32 mFwd[3] = { 0.f, 0.f, -1.f };
    F32 mUp[3] = { 0.f, 1.f, 0.f };
    F32 mColor[3] = { 0.f, 0.f, 0.f }; // linear colour * global scale * EV scale
    F32 mRadius = 0.f;                 // getLightRadius(); reach is 1.5x
    F32 mFalloff = 0.f;
    F32 mScale[3] = { 0.f, 0.f, 0.f };
    bool mSpot = false;
    F32 mSpotParams[3] = { 0.f, 0.f, 0.f };
    LLUUID mTexId;
    bool mNoShadow = false;
    GoboSample mGobo;
};

struct LightEntry
{
    LLUUID mId;
    U64 mIdTag = 0;
    U64 mSeq = 0; // monotonic creation order (resumable recount cursor)
    bool mEligible = false;
    bool mSpot = false;
    F32 mPos[3] = { 0.f, 0.f, 0.f };
    F32 mPubPos[3] = { 0.f, 0.f, 0.f }; // published at the XFORM settle
    F32 mRadius = 0.f;                  // raw getLightRadius()
    F32 mReach = 0.f;                   // 1.5 * mRadius
    ALCineLiveProbeRefresh::Signature mSig[LS_COUNT];
    ALCineLiveProbeRefresh::StickyHash mSticky[LS_COUNT];
    U64 mExact[LS_COUNT] = { 0, 0, 0 };
    U64 mH[LS_COUNT] = { 0, 0, 0 };
    std::vector<F32> mPubAcc[2]; // published XFORM / PHOTO accumulators
    U64 mPubH[2] = { 0, 0 };     // published XFORM / PHOTO hashes
    Debounce mDeb[2];            // XFORM / PHOTO motion
    bool mSeen = false;
    // Tracked only for the Live tokens (no ordinary probe reaches it): its events
    // are discarded so camera / subject motion never produces light events.
    bool mSilent = false;
};

constexpr U8 LSR_ADDED = 1;
constexpr U8 LSR_STRUCT = 2;
constexpr U8 LSR_MOTION = 4;

inline void lightFillSignatures(LightEntry& e, const LightSample& s)
{
    ALCineLiveProbeRefresh::Signature& x = e.mSig[LS_XFORM];
    x.clear();
    x.addAbs3(s.mPos, 0.05f); // fields 0-2 = position BY CONTRACT (shift())
    x.addAngle(s.mFwd, 0.25f);
    x.addAngle(s.mUp, 0.25f);

    ALCineLiveProbeRefresh::Signature& p = e.mSig[LS_PHOTO];
    p.clear();
    p.addColor(s.mColor, 0.01f, 1e-3f);

    ALCineLiveProbeRefresh::Signature& t = e.mSig[LS_STRUCT];
    t.clear();
    t.addExact(s.mEligible);
    t.addAbs(s.mRadius, 0.05f);
    t.addAbs(s.mFalloff, 0.01f);
    t.addAbs3(s.mScale, 0.005f);
    t.addExact(s.mSpot);
    t.addAbs3(s.mSpotParams, 0.00436f);
    t.addExact(s.mTexId);
    t.addExact(s.mNoShadow);
    t.addExact(static_cast<U64>(static_cast<U32>(s.mGobo.mPattern)));
    t.addExact(static_cast<U64>(static_cast<U32>(s.mGobo.mAnimMode)));
    t.addAbs(s.mGobo.mSpeed, 1e-3f);
    t.addAbs(s.mGobo.mZoom, 1e-3f);
    t.addAbs(s.mGobo.mDispersion, 1e-3f);
    t.addColor(s.mGobo.mTint, 0.005f, 1e-3f);
    for (S32 i = 0; i < 4; ++i)
    {
        t.addAbs(s.mGobo.mParams[i], 1e-3f);
    }
}

// Publish the current value of motion class `which` (0 = XFORM, 1 = PHOTO).
inline void lightPublish(LightEntry& e, S32 which)
{
    if (which == 0)
    {
        for (S32 i = 0; i < 3; ++i)
        {
            e.mPubPos[i] = e.mPos[i];
        }
        e.mPubAcc[0] = e.mSticky[LS_XFORM].mAcc;
        e.mPubH[0] = e.mH[LS_XFORM];
    }
    else
    {
        e.mPubAcc[1] = e.mSticky[LS_PHOTO].mAcc;
        e.mPubH[1] = e.mH[LS_PHOTO];
    }
}

// One frame of the diff for one light. `is_new` entries are initialised from
// the sample. Returns LSR_* flags; events are appended to `out`.
inline U8 lightStep(LightEntry& e, const LightSample& s, bool is_new, U64 serial,
                    F64 now, F32 dt, const DebounceConfig& dc, std::vector<Event>& out)
{
    lightFillSignatures(e, s);
    U64 h[LS_COUNT];
    for (S32 k = 0; k < LS_COUNT; ++k)
    {
        h[k] = e.mSticky[k].update(e.mSig[k]);
        e.mExact[k] = e.mSig[k].mExact;
    }
    const F32 reach = 1.5f * s.mRadius;
    U8 res = 0;
    F32 mn[3], mx[3];

    if (is_new)
    {
        e.mId = s.mId;
        e.mIdTag = idTagOf(s.mId);
        e.mEligible = s.mEligible;
        e.mSpot = s.mSpot;
        e.mRadius = s.mRadius;
        e.mReach = reach;
        for (S32 i = 0; i < 3; ++i)
        {
            e.mPos[i] = s.mPos[i];
        }
        for (S32 k = 0; k < LS_COUNT; ++k)
        {
            e.mH[k] = h[k];
        }
        lightPublish(e, 0);
        lightPublish(e, 1);
        if (s.mEligible)
        {
            boxSphere(mn, mx, s.mPos, reach);
            Event ev = makeEvent(mn, mx, R_LIGHT, C_LIGHT, serial);
            ev.mSpot = s.mSpot;
            out.push_back(ev);
        }
        return LSR_ADDED;
    }

    const bool relevant = s.mEligible || e.mEligible;
    const F32 max_reach = std::max(e.mReach, reach);
    const bool spot = s.mSpot || e.mSpot;

    if (h[LS_STRUCT] != e.mH[LS_STRUCT] && relevant)
    {
        F32 mn2[3], mx2[3];
        boxSphere(mn, mx, e.mPos, max_reach);
        boxSphere(mn2, mx2, s.mPos, max_reach);
        boxUnion(mn, mx, mn2, mx2);
        Event ev = makeEvent(mn, mx, R_LIGHT, C_LIGHT, serial);
        ev.mSpot = spot;
        out.push_back(ev);
        res = static_cast<U8>(res | LSR_STRUCT);
    }
    if (relevant)
    {
        if (h[LS_XFORM] != e.mH[LS_XFORM])
        {
            F32 mn2[3], mx2[3];
            boxSphere(mn, mx, e.mPos, max_reach);
            boxSphere(mn2, mx2, s.mPos, max_reach);
            boxUnion(mn, mx, mn2, mx2);
            debOnMotion(dc, e.mDeb[0], now, dt, mn, mx, R_LIGHT, C_LIGHT, spot, serial);
            res = static_cast<U8>(res | LSR_MOTION);
        }
        if (h[LS_PHOTO] != e.mH[LS_PHOTO])
        {
            boxSphere(mn, mx, s.mPos, max_reach);
            debOnMotion(dc, e.mDeb[1], now, dt, mn, mx, R_LIGHT, C_LIGHT, spot, serial);
            res = static_cast<U8>(res | LSR_MOTION);
        }
    }

    e.mEligible = s.mEligible;
    e.mSpot = s.mSpot;
    e.mRadius = s.mRadius;
    e.mReach = reach;
    for (S32 i = 0; i < 3; ++i)
    {
        e.mPos[i] = s.mPos[i];
    }
    for (S32 k = 0; k < LS_COUNT; ++k)
    {
        e.mH[k] = h[k];
    }
    if (!relevant)
    {
        // Nothing renders from it: keep the published values current.
        lightPublish(e, 0);
        lightPublish(e, 1);
    }
    return res;
}

// Emit due settle events of one entry and publish the settled class.
inline U32 lightSettleDue(LightEntry& e, F64 now, std::vector<Event>& out)
{
    U32 n = 0;
    for (S32 which = 0; which < 2; ++which)
    {
        if (debDue(e.mDeb[which], now))
        {
            out.push_back(debTake(e.mDeb[which]));
            lightPublish(e, which);
            ++n;
        }
    }
    return n;
}

// A light left the diff (removed, or left the prefilter): one discrete event
// over its stored and published positions merged with any pending motion.
inline void lightLeave(const LightEntry& e, U64 serial, std::vector<Event>& out)
{
    if (!e.mEligible)
    {
        return;
    }
    F32 mn[3], mx[3], mn2[3], mx2[3];
    boxSphere(mn, mx, e.mPos, e.mReach);
    boxSphere(mn2, mx2, e.mPubPos, e.mReach);
    boxUnion(mn, mx, mn2, mx2);
    Event ev = makeEvent(mn, mx, R_LIGHT, C_LIGHT, serial);
    ev.mSpot = e.mSpot;
    debMergeInto(e.mDeb[0], ev);
    debMergeInto(e.mDeb[1], ev);
    out.push_back(ev);
}

// Region crossing: positions, position accumulators and every stored hash move
// together (fields 0-2 of the XFORM signature are the position), so a crossing
// produces no light event.
inline void lightShift(LightEntry& e, const F32 off[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        e.mPos[i] += off[i];
        e.mPubPos[i] += off[i];
    }
    ALCineLiveProbeRefresh::StickyHash& sx = e.mSticky[LS_XFORM];
    for (S32 i = 0; i < 3; ++i)
    {
        if (sx.mAcc.size() > static_cast<size_t>(i))
        {
            sx.mAcc[static_cast<size_t>(i)] += off[i];
        }
        if (e.mPubAcc[0].size() > static_cast<size_t>(i))
        {
            e.mPubAcc[0][static_cast<size_t>(i)] += off[i];
        }
    }
    e.mH[LS_XFORM] = stickyDigest(sx, e.mExact[LS_XFORM]);
    e.mPubH[0] = stickyDigest(e.mPubAcc[0], sx.mLayout, e.mExact[LS_XFORM]);
    for (S32 which = 0; which < 2; ++which)
    {
        boxOffset(e.mDeb[which].mMin, e.mDeb[which].mMax, off);
    }
}

// ---------------------------------------------------------------------------
// Amortised, resumable cap-rule recount (CX3/CX4 + correction 2): at most
// `budget` (light, probe) pair tests per frame, cursor persists across frames at
// pair level, priority and regular work alternate (half the budget each when
// both have work) and the regular job always takes the probe with the oldest
// recount, so every probe is recounted within a bounded number of frames.
// ---------------------------------------------------------------------------
constexpr U32 kRecountPairBudget = 8192;

struct RecountProbe
{
    Record* mRec = nullptr;
    F32 mOrigin[3] = { 0.f, 0.f, 0.f };
    F32 mMaxDist = 0.f; // min(RenderFarClip, probe draw distance)
};

inline void markRecountDue(Record& r)
{
    r.mRecountDue = true;
    ++r.mRecountDirtyEpoch;
}

struct RecountJob
{
    bool mActive = false;
    bool mHasCursor = false;
    U32 mId = 0;
    U64 mCursorSeq = 0;
    U32 mAcc = 0;
    U32 mEpoch = 0;
};

class Recount
{
public:
    void reset()
    {
        mPrio = RecountJob();
        mReg = RecountJob();
        mCompleted = 0;
    }

    U32 completed() const { return mCompleted; }

    // `lights` must be sorted ascending by mSeq. Returns pair tests used.
    U32 run(std::vector<RecountProbe>& probes, const std::vector<const LightEntry*>& lights,
            U32 budget, U64 serial, S32 cap)
    {
        // Budget-driven: keep starting / finishing jobs until the pair budget is
        // spent or every probe has been recounted this frame. Priority and regular
        // work alternate (each gets half of what is left while both have work).
        U32 used = 0;
        const U32 max_rounds = static_cast<U32>(probes.size()) * 2u + 4u;
        for (U32 round = 0; round < max_rounds && used < budget; ++round)
        {
            assign(mPrio, probes, true, serial);
            assign(mReg, probes, false, serial);
            if (!mPrio.mActive && !mReg.mActive)
            {
                break; // everything has been recounted this frame
            }
            const U32 remain = budget - used;
            const bool both = mPrio.mActive && mReg.mActive;
            U32 step = advance(mPrio, probes, lights, both ? remain / 2 : remain, serial, cap);
            step += advance(mReg, probes, lights, remain > step ? remain - step : 0u, serial, cap);
            used += step;
        }        return used;
    }

private:
    static RecountProbe* find(std::vector<RecountProbe>& probes, U32 id)
    {
        for (RecountProbe& p : probes)
        {
            if (p.mRec && p.mRec->mId == id)
            {
                return &p;
            }
        }
        return nullptr;
    }

    // Pick the oldest-recounted probe (priority: only probes with the recount
    // flag set) that no job is working on.
    void assign(RecountJob& job, std::vector<RecountProbe>& probes, bool priority, U64 serial)
    {
        if (job.mActive)
        {
            return;
        }
        const RecountJob& other = (&job == &mPrio) ? mReg : mPrio;
        RecountProbe* best = nullptr;
        for (RecountProbe& p : probes)
        {
            if (!p.mRec || (other.mActive && other.mId == p.mRec->mId))
            {
                continue;
            }
            if (p.mRec->mLastRecountSerial == serial)
            {
                continue; // already recounted this frame
            }
            if (priority && !p.mRec->mRecountDue)
            {
                continue;
            }
            if (!best || p.mRec->mLastRecountSerial < best->mRec->mLastRecountSerial ||
                (p.mRec->mLastRecountSerial == best->mRec->mLastRecountSerial &&
                 p.mRec->mId < best->mRec->mId))
            {
                best = &p;
            }
        }
        if (best)
        {
            job = RecountJob();
            job.mActive = true;
            job.mId = best->mRec->mId;
            job.mEpoch = best->mRec->mRecountDirtyEpoch;
        }
    }

    U32 advance(RecountJob& job, std::vector<RecountProbe>& probes,
                const std::vector<const LightEntry*>& lights, U32 budget, U64 serial,
                S32 cap)
    {
        if (!job.mActive)
        {
            return 0;
        }
        RecountProbe* probe = find(probes, job.mId);
        if (!probe)
        {
            job = RecountJob(); // the probe left; nothing to finish
            return 0;
        }
        size_t idx = 0;
        if (job.mHasCursor)
        {
            idx = static_cast<size_t>(std::upper_bound(
                lights.begin(), lights.end(), job.mCursorSeq,
                [](U64 seq, const LightEntry* l) { return seq < l->mSeq; }) - lights.begin());
        }
        U32 used = 0;
        while (idx < lights.size() && used < budget)
        {
            const LightEntry& l = *lights[idx];
            ++used;
            if (l.mEligible)
            {
                F32 d2 = 0.f;
                for (S32 i = 0; i < 3; ++i)
                {
                    const F32 d = l.mPos[i] - probe->mOrigin[i];
                    d2 += d * d;
                }
                if (l.mSpot || std::sqrt(d2) - l.mReach < probe->mMaxDist)
                {
                    ++job.mAcc;
                }
            }
            job.mCursorSeq = l.mSeq;
            job.mHasCursor = true;
            ++idx;
        }
        if (idx < lights.size())
        {
            return used; // budget exhausted; resume next frame
        }
        Record& r = *probe->mRec;
        r.mLocalLightsPrev = r.mLocalLights;
        r.mLocalLights = static_cast<U16>(std::min<U32>(job.mAcc, 0xFFFFu));
        r.mLastRecountSerial = serial;
        if (static_cast<S32>(r.mLocalLightsPrev) > cap || static_cast<S32>(r.mLocalLights) > cap)
        {
            r.mOverCapSerial = serial;
        }
        if (r.mRecountDirtyEpoch == job.mEpoch)
        {
            r.mRecountDue = false; // nothing re-dirtied it while it was counted
        }
        ++mCompleted;
        job = RecountJob();
        return used;
    }

    RecountJob mPrio;
    RecountJob mReg;
    U32 mCompleted = 0;
};

// ---------------------------------------------------------------------------
// Realtime (closest dynamic) probe slicing cursor. Same pass-kind rule as
// ALCineLiveProbeRefresh (a pass is radiance iff the last completed pass was
// irradiance); an identity change resets it and the next pass starts at face 0.
// ---------------------------------------------------------------------------
struct SliceCursor
{
    U32 mId = 0;
    S32 mCube = -1;
    bool mActive = false;
    U8 mFace = 0;
    bool mRadiance = false;
    bool mLastPassIrr = false;
    U64 mPassStartSerial = 0;

    void reset()
    {
        *this = SliceCursor();
    }
};

// Bind the cursor to a probe identity; a change drops any partial pass.
inline void sliceBind(SliceCursor& c, U32 id, S32 cube)
{
    if (c.mId != id || c.mCube != cube)
    {
        c = SliceCursor();
        c.mId = id;
        c.mCube = cube;
    }
}

// Start a pass at face 0 when none is running.
inline void sliceBegin(SliceCursor& c, U64 serial)
{
    if (!c.mActive)
    {
        c.mActive = true;
        c.mFace = 0;
        c.mRadiance = c.mLastPassIrr;
        c.mPassStartSerial = serial;
    }
}

// The Live probe wrote the secondary scratch (FULL / budget / Every-frame pass):
// whatever faces a partial sliced pass left there are gone, so the cursor must not
// resume (it would filter a mix of both captures). The next pass starts at face 0.
inline void sliceInvalidate(SliceCursor& c)
{
    c.reset();
}

// After each captured face.
inline ALCineLiveProbeRefresh::PassEnd sliceAdvance(SliceCursor& c)
{
    c.mFace = static_cast<U8>(c.mFace + 1);
    if (c.mFace < 6)
    {
        return ALCineLiveProbeRefresh::PassEnd::NONE;
    }
    c.mActive = false;
    c.mLastPassIrr = !c.mRadiance;
    return c.mRadiance ? ALCineLiveProbeRefresh::PassEnd::RADIANCE
                       : ALCineLiveProbeRefresh::PassEnd::IRRADIANCE;
}

// ---------------------------------------------------------------------------
// Refresh-all capture barrier (membership by stable probe id; completion needs
// a capture that STARTED after arming).
// ---------------------------------------------------------------------------
struct Barrier
{
    enum class LiveState : U8 { NONE, UNAVAILABLE, PENDING, IRR_DONE, DONE, REMOVED };
    enum class Terminal : U8 { NONE, DONE, INCOMPLETE, CANCELLED_DISABLED, CANCELLED_RESET };
    enum class MState : U8 { PENDING, DONE, DROPPED };
    struct Member
    {
        U32 mId = 0;
        MState mState = MState::PENDING;
        bool mIrrSeen = false; // REALTIME: a post-arm irradiance pass completed
    };

    static constexpr F64 kTimeoutSec = 30.0;

    bool mActive = false;
    U64 mArmSerial = 0;
    F64 mArmTime = 0.0;
    F64 mUnpausedSec = 0.0; // unpaused time since arming (the timeout clock)
    F64 mLastTick = -1.0;
    bool mPaused = false;
    std::vector<Member> mMembers;
    LiveState mLive = LiveState::NONE;
    U32 mLiveId = 0;
    // The Live arm point is a monotonic OP, not a frame serial: a replacement
    // re-arm at serial N would otherwise reject a pass that starts later in the
    // SAME frame (spec correction 1).
    U64 mLiveArmOp = 0;
    U64 mLivePassStartOp = 0; // budget-pass start op (manager bookkeeping)
    U32 mLiveRearms = 0;
    Terminal mTerminal = Terminal::NONE;
    F64 mTerminalTime = -1.0;
};

inline void barrierArm(Barrier& b, U64 serial, F64 now, const std::vector<U32>& ids,
                       Barrier::LiveState live, U32 live_id, U64 arm_op)
{
    b = Barrier();
    b.mActive = true;
    b.mArmSerial = serial;
    b.mArmTime = now;
    b.mLastTick = now;
    for (const U32 id : ids)
    {
        Barrier::Member m;
        m.mId = id;
        b.mMembers.push_back(m);
    }
    b.mLive = live;
    b.mLiveId = live_id;
    b.mLiveArmOp = arm_op;
}

inline Barrier::Member* barrierFind(Barrier& b, U32 id)
{
    for (Barrier::Member& m : b.mMembers)
    {
        if (m.mId == id)
        {
            return &m;
        }
    }
    return nullptr;
}

inline bool barrierIsMember(const Barrier& b, U32 id)
{
    if (!b.mActive)
    {
        return false;
    }
    for (const Barrier::Member& m : b.mMembers)
    {
        if (m.mId == id)
        {
            return m.mState == Barrier::MState::PENDING;
        }
    }
    return false;
}

// ORDINARY member: done only by an ACKED transaction that started after arming.
inline void barrierMemberAck(Barrier& b, U32 id, U64 txn_serial)
{
    Barrier::Member* m = b.mActive ? barrierFind(b, id) : nullptr;
    if (m && m->mState == Barrier::MState::PENDING && txn_serial >= b.mArmSerial)
    {
        m->mState = Barrier::MState::DONE;
    }
}

// Deleted, lost its slot, or became irrelevant.
inline void barrierMemberDropped(Barrier& b, U32 id)
{
    Barrier::Member* m = b.mActive ? barrierFind(b, id) : nullptr;
    if (m && m->mState == Barrier::MState::PENDING)
    {
        m->mState = Barrier::MState::DROPPED;
    }
}

// REALTIME member: a post-arm irradiance pass then a radiance pass whose face
// 0 ran after arming.
inline void barrierRealtimePass(Barrier& b, U32 id, bool radiance, U64 pass_start_serial)
{
    Barrier::Member* m = b.mActive ? barrierFind(b, id) : nullptr;
    if (!m || m->mState != Barrier::MState::PENDING || pass_start_serial < b.mArmSerial)
    {
        return;
    }
    if (!radiance)
    {
        m->mIrrSeen = true;
    }
    else if (m->mIrrSeen)
    {
        m->mState = Barrier::MState::DONE;
    }
}

// Called every flush while armed. `designated`: a Live probe exists; `available`:
// it has a cube and is relevant; `id`: its record id; `now_op`: the current op.
inline void barrierLiveDesignation(Barrier& b, bool designated, bool available,
                                   U32 id, U64 now_op)
{
    if (!b.mActive)
    {
        return;
    }
    const bool in_flight = b.mLive == Barrier::LiveState::PENDING ||
                           b.mLive == Barrier::LiveState::IRR_DONE;
    if (!designated)
    {
        if (in_flight)
        {
            b.mLive = Barrier::LiveState::REMOVED;
        }
        return;
    }
    if (!in_flight)
    {
        return;
    }
    if (id != b.mLiveId)
    {
        // Replaced: re-arm on the new probe. Passes from before this op (of the
        // old or the new probe) never count.
        b.mLiveId = id;
        b.mLiveArmOp = now_op;
        ++b.mLiveRearms;
        b.mLive = available ? Barrier::LiveState::PENDING : Barrier::LiveState::UNAVAILABLE;
        return;
    }
    if (!available)
    {
        b.mLive = Barrier::LiveState::UNAVAILABLE;
    }
}

// A finished Live pass (FULL, budget or Every-frame single frame).
inline void barrierLivePass(Barrier& b, U32 probe_id, bool radiance, U64 start_op)
{
    if (!b.mActive || probe_id != b.mLiveId || start_op <= b.mLiveArmOp)
    {
        return;
    }
    if (b.mLive == Barrier::LiveState::PENDING && !radiance)
    {
        b.mLive = Barrier::LiveState::IRR_DONE;
    }
    else if (b.mLive == Barrier::LiveState::IRR_DONE && radiance)
    {
        b.mLive = Barrier::LiveState::DONE;
    }
}

inline U32 barrierTotal(const Barrier& b)
{
    return static_cast<U32>(b.mMembers.size());
}

inline U32 barrierCount(const Barrier& b, Barrier::MState s)
{
    U32 n = 0;
    for (const Barrier::Member& m : b.mMembers)
    {
        if (m.mState == s)
        {
            ++n;
        }
    }
    return n;
}

inline bool barrierLiveSettled(const Barrier& b)
{
    return b.mLive == Barrier::LiveState::NONE ||
           b.mLive == Barrier::LiveState::UNAVAILABLE ||
           b.mLive == Barrier::LiveState::DONE ||
           b.mLive == Barrier::LiveState::REMOVED;
}

// Advance the terminal logic. `paused` stops the timeout clock (EEP transition).
inline void barrierTick(Barrier& b, F64 now, bool paused)
{
    if (!b.mActive)
    {
        return;
    }
    b.mPaused = paused;
    if (b.mLastTick >= 0.0 && !paused)
    {
        b.mUnpausedSec += std::max(0.0, now - b.mLastTick);
    }
    b.mLastTick = now;
    if (barrierCount(b, Barrier::MState::PENDING) == 0 && barrierLiveSettled(b))
    {
        b.mActive = false;
        b.mTerminal = Barrier::Terminal::DONE;
        b.mTerminalTime = now;
    }
    else if (b.mUnpausedSec >= Barrier::kTimeoutSec)
    {
        b.mActive = false;
        b.mTerminal = Barrier::Terminal::INCOMPLETE;
        b.mTerminalTime = now;
    }
}

inline void barrierCancel(Barrier& b, bool reset, F64 now)
{
    if (b.mActive)
    {
        b.mActive = false;
        b.mTerminal = reset ? Barrier::Terminal::CANCELLED_RESET
                            : Barrier::Terminal::CANCELLED_DISABLED;
        b.mTerminalTime = now;
    }
}

inline std::string barrierLiveText(const Barrier& b)
{
    switch (b.mLive)
    {
        case Barrier::LiveState::NONE:        return std::string();
        case Barrier::LiveState::UNAVAILABLE: return "Live: unavailable";
        case Barrier::LiveState::PENDING:
        case Barrier::LiveState::IRR_DONE:
            return b.mLiveRearms > 0 ? "Live: replaced -> re-armed" : "Live: pending";
        case Barrier::LiveState::DONE:        return "Live: refreshed";
        case Barrier::LiveState::REMOVED:     return "Live: removed";
    }
    return std::string();
}

// The Lightbox readout. Never says "All" unless every member finished.
inline std::string barrierReadout(const Barrier& b)
{
    const U32 total = barrierTotal(b);
    const U32 done = barrierCount(b, Barrier::MState::DONE);
    const U32 dropped = barrierCount(b, Barrier::MState::DROPPED);
    std::string live = barrierLiveText(b);
    if (!live.empty())
    {
        live = ", " + live;
    }
    if (b.mActive)
    {
        std::string out = "Refreshing probes " + std::to_string(done + dropped) + "/" +
                          std::to_string(total);
        if (b.mPaused)
        {
            out += " (paused)";
        }
        return out + live;
    }
    switch (b.mTerminal)
    {
        case Barrier::Terminal::NONE:
            return std::string();
        case Barrier::Terminal::DONE:
            if (dropped == 0)
            {
                return "All probes refreshed (" + std::to_string(done) + ")" + live;
            }
            return "All remaining probes refreshed (" + std::to_string(done) + " of " +
                   std::to_string(total) + ", " + std::to_string(dropped) + " dropped)" + live;
        case Barrier::Terminal::INCOMPLETE:
            return "Incomplete: " + std::to_string(done) + "/" + std::to_string(total) +
                   " refreshed" + live;
        case Barrier::Terminal::CANCELLED_DISABLED:
            return "Refresh all: cancelled (disabled)";
        case Barrier::Terminal::CANCELLED_RESET:
            return "Refresh all: cancelled (reset)";
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Environment generation. The sampler (manager) gathers exactly the getters of
// the Live probe's H (llreflectionmapmanager.cpp, sampleCinematicH) into this
// POD; the builder appends them in the same fixed order with the same
// tolerances. Two documented differences: the sun/moon direction tolerance is a
// parameter (HEAD: 0.1 deg), and the blend factors may be zeroed when the
// textures they blend are not mixing.
// ---------------------------------------------------------------------------
struct V3
{
    F32 mV[3] = { 0.f, 0.f, 0.f };
};

struct SkySample
{
    bool mValid = false;
    V3 mSunDir, mMoonDir, mSunlight, mMoonlight, mCloudColor, mAmbient, mBlueDensity,
        mBlueHorizon, mGlow, mCloudPosDensity1, mCloudPosDensity2;
    F32 mHazeDensity = 0.f, mDensityMultiplier = 0.f, mDistanceMultiplier = 0.f,
        mCloudScale = 0.f, mStarBrightness = 0.f, mDropletRadius = 0.f, mMaxY = 0.f;
    F32 mHazeHorizon = 0.f, mCloudShadow = 0.f, mCloudVariance = 0.f, mMoonBrightness = 0.f,
        mMoistureLevel = 0.f, mIceLevel = 0.f, mReflectionAmbiance = 0.f, mGamma = 0.f,
        mSunMoonGlowFactor = 0.f;
    F32 mSunScale = 0.f, mMoonScale = 0.f;
    F32 mBlendFactor = 0.f;
    bool mIsSunUp = false;
    LLUUID mSunTex, mMoonTex, mCloudNoiseTex, mBloomTex, mRainbowTex, mHaloTex,
        mNextSunTex, mNextMoonTex, mNextCloudNoiseTex;
};

struct WaterSample
{
    bool mValid = false;
    V3 mFogColor;
    F32 mFogDensity = 0.f, mFogMod = 0.f, mFresnelScale = 0.f, mFresnelOffset = 0.f,
        mScaleAbove = 0.f, mScaleBelow = 0.f, mBlendFactor = 0.f, mBlurMultiplier = 0.f;
    F32 mWave1[2] = { 0.f, 0.f };
    F32 mWave2[2] = { 0.f, 0.f };
    V3 mNormalScale;
    F32 mRenderWaterHeight = 0.f;
    LLUUID mNormalMap, mNextNormalMap, mTransparent, mNextTransparent;
};

struct EnvSettings
{
    F32 mSunEV = 0.f, mMoonEV = 0.f, mLocalLightEV = 0.f, mShadowLiftEV = 0.f,
        mSunKelvin = 6500.f;
    F32 mSunTint[3] = { 1.f, 1.f, 1.f };
    F32 mMoonTint[3] = { 1.f, 1.f, 1.f };
    F32 mSunTintStrength = 1.f, mMoonTintStrength = 1.f;
    bool mMoonLinked = true;
    bool mLocalLightIncludeRig = false;
    S32 mShadowDetail = 2;
    S32 mLocalLightCount = 256;
    bool mAutoAdjustLegacy = false;
};

struct EnvSample
{
    SkySample mSky;
    WaterSample mWater;
    EnvSettings mSet;
};

inline bool skyTexturesMixing(const SkySample& s)
{
    return s.mSunTex != s.mNextSunTex || s.mMoonTex != s.mNextMoonTex ||
           s.mCloudNoiseTex != s.mNextCloudNoiseTex;
}

inline bool waterTexturesMixing(const WaterSample& w)
{
    return w.mNormalMap != w.mNextNormalMap || w.mTransparent != w.mNextTransparent;
}

inline void appendEnvironmentFields(ALCineLiveProbeRefresh::Signature& sig,
                                    const EnvSample& e, F32 sun_moon_deg,
                                    bool blend_only_when_mixing)
{
    // 3. Sky
    const SkySample& sky = e.mSky;
    if (!sky.mValid)
    {
        sig.addExact(false);
    }
    else
    {
        sig.addExact(true);
        sig.addAngle(sky.mSunDir.mV, sun_moon_deg);
        sig.addAngle(sky.mMoonDir.mV, sun_moon_deg);
        sig.addColor(sky.mSunlight.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mMoonlight.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mCloudColor.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mAmbient.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mBlueDensity.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mBlueHorizon.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mGlow.mV, 0.005f, 1e-4f);
        sig.addColor(sky.mCloudPosDensity1.mV, 0.005f, 1e-3f);
        sig.addColor(sky.mCloudPosDensity2.mV, 0.005f, 1e-3f);
        sig.addRel(sky.mHazeDensity, 0.005f, 1e-4f);
        sig.addRel(sky.mDensityMultiplier, 0.01f, 1e-7f);
        sig.addRel(sky.mDistanceMultiplier, 0.005f, 1e-3f);
        sig.addRel(sky.mCloudScale, 0.005f, 1e-4f);
        sig.addRel(sky.mStarBrightness, 0.005f, 1e-3f);
        sig.addRel(sky.mDropletRadius, 0.005f, 0.01f);
        sig.addRel(sky.mMaxY, 0.005f, 1.f);
        sig.addAbs(sky.mHazeHorizon, 1e-3f);
        sig.addAbs(sky.mCloudShadow, 1e-3f);
        sig.addAbs(sky.mCloudVariance, 1e-3f);
        sig.addAbs(sky.mMoonBrightness, 1e-3f);
        sig.addAbs(sky.mMoistureLevel, 1e-3f);
        sig.addAbs(sky.mIceLevel, 1e-3f);
        sig.addAbs(sky.mReflectionAmbiance, 1e-3f);
        sig.addAbs(sky.mGamma, 1e-3f);
        sig.addAbs(sky.mSunMoonGlowFactor, 1e-3f);
        // Sun / moon disc size: drawn into the cube faces and edited live by
        // Personal Lighting without a manager reset.
        sig.addRel(sky.mSunScale, 0.005f, 1e-3f);
        sig.addRel(sky.mMoonScale, 0.005f, 1e-3f);
        sig.addAbs((blend_only_when_mixing && !skyTexturesMixing(sky)) ? 0.f : sky.mBlendFactor,
                   1e-3f);
        sig.addExact(sky.mIsSunUp);
        sig.addExact(sky.mSunTex);
        sig.addExact(sky.mMoonTex);
        sig.addExact(sky.mCloudNoiseTex);
        sig.addExact(sky.mBloomTex);
        sig.addExact(sky.mRainbowTex);
        sig.addExact(sky.mHaloTex);
        sig.addExact(sky.mNextSunTex);
        sig.addExact(sky.mNextMoonTex);
        sig.addExact(sky.mNextCloudNoiseTex);
    }

    // 4. Water
    const WaterSample& water = e.mWater;
    if (!water.mValid)
    {
        sig.addExact(false);
    }
    else
    {
        sig.addExact(true);
        sig.addColor(water.mFogColor.mV, 0.005f, 1e-4f);
        sig.addRel(water.mFogDensity, 0.005f, 1e-4f);
        sig.addAbs(water.mFogMod, 1e-3f);
        sig.addAbs(water.mFresnelScale, 1e-3f);
        sig.addAbs(water.mFresnelOffset, 1e-3f);
        sig.addAbs(water.mScaleAbove, 1e-3f);
        sig.addAbs(water.mScaleBelow, 1e-3f);
        sig.addAbs((blend_only_when_mixing && !waterTexturesMixing(water)) ? 0.f
                                                                           : water.mBlendFactor,
                   1e-3f);
        sig.addAbs(water.mBlurMultiplier, 1e-4f);
        sig.addAbs(water.mWave1[0], 1e-3f);
        sig.addAbs(water.mWave1[1], 1e-3f);
        sig.addAbs(water.mWave2[0], 1e-3f);
        sig.addAbs(water.mWave2[1], 1e-3f);
        sig.addAbs3(water.mNormalScale.mV, 1e-3f);
        sig.addAbs(water.mRenderWaterHeight, 0.01f);
        sig.addExact(water.mNormalMap);
        sig.addExact(water.mNextNormalMap);
        sig.addExact(water.mTransparent);
        sig.addExact(water.mNextTransparent);
    }

    // 5. Capture-relevant settings. GI / ambient sampling EVs are identity
    // during captures and are deliberately excluded.
    const EnvSettings& st = e.mSet;
    sig.addAbs(st.mSunEV, 0.01f);
    sig.addAbs(st.mMoonEV, 0.01f);
    sig.addAbs(st.mLocalLightEV, 0.01f);
    sig.addAbs(st.mShadowLiftEV, 0.01f);
    sig.addAbs(st.mSunKelvin, 10.f);
    sig.addAbs3(st.mSunTint, 1.f / 512.f);
    sig.addAbs3(st.mMoonTint, 1.f / 512.f);
    sig.addAbs(st.mSunTintStrength, 1e-3f);
    sig.addAbs(st.mMoonTintStrength, 1e-3f);
    sig.addExact(st.mMoonLinked);
    sig.addExact(st.mLocalLightIncludeRig);
    sig.addExact(static_cast<U64>(static_cast<U32>(st.mShadowDetail)));
    sig.addExact(static_cast<U64>(static_cast<U32>(st.mLocalLightCount)));
    sig.addExact(st.mAutoAdjustLegacy);
}

// The ordinary probes' environment tail (fixed layout): what else the capture
// depends on beyond the Live probe's environment.
constexpr S32 kMaxRimSlots = 8;
constexpr S32 kMaxRimLights = 8;

struct OrdTailSample
{
    F32 mDrawDistance = 64.f;      // RenderReflectionProbeDrawDistance
    F32 mListRange = 64.f;         // min(RenderFarClip, draw distance)
    S32 mProbeDetail = 1;
    S32 mProbeLevel = 3;
    S32 mLocalLightCount = 256;
    F32 mMaxLocalLightAmbiance = 8.f;
    bool mAttachedLights = true;
    bool mBdToggles = false;
    bool mBdOwn = true;
    bool mBdOthers = true;
    bool mBdWorld = true;
    bool mBdProjectors = true;
    F32 mGlobalLightScale = 1.f;
    bool mRimIncludeProbes = false;
    bool mRimEnabled = false;
    F32 mRimMasterGain = 1.f, mRimBackSoftness = 0.5f, mRimRoughnessSoften = 0.5f,
        mRimShadow = 1.f, mRimTronMix = 1.f, mRimTint = 0.25f;
    bool mRimDebugOnly = false;
    bool mRimIncludeAlpha = true;
    S32 mRimTronMode = 0;
    S32 mRimTronColorSource = 0;
    S32 mSlotCount = 0;
    S32 mLightCount = 0;
    bool mSlotEnabled[kMaxRimSlots] = {};
    F32 mRimParams[kMaxRimSlots][kMaxRimLights][4] = {};
};

inline void appendOrdinaryTail(ALCineLiveProbeRefresh::Signature& sig, const OrdTailSample& t)
{
    sig.addAbs(t.mDrawDistance, 0.5f);
    sig.addAbs(t.mListRange, 0.5f);
    sig.addExact(static_cast<U64>(static_cast<U32>(t.mProbeDetail)));
    sig.addExact(static_cast<U64>(static_cast<U32>(t.mProbeLevel)));
    sig.addExact(static_cast<U64>(static_cast<U32>(t.mLocalLightCount)));
    sig.addAbs(t.mMaxLocalLightAmbiance, 1e-3f); // drives mLightScale
    sig.addExact(t.mAttachedLights);
    sig.addExact(t.mBdToggles);
    sig.addExact(t.mBdOwn);
    sig.addExact(t.mBdOthers);
    sig.addExact(t.mBdWorld);
    sig.addExact(t.mBdProjectors);
    sig.addAbs(t.mGlobalLightScale, 1e-3f);

    // Rig Rim: ALWAYS the whole block (fixed layout); values are zeroed unless
    // both switches are on.
    sig.addExact(t.mRimIncludeProbes);
    sig.addExact(t.mRimEnabled);
    const bool rim = t.mRimIncludeProbes && t.mRimEnabled;
    sig.addAbs(rim ? t.mRimMasterGain : 0.f, 1e-3f);
    sig.addAbs(rim ? t.mRimBackSoftness : 0.f, 1e-3f);
    sig.addAbs(rim ? t.mRimRoughnessSoften : 0.f, 1e-3f);
    sig.addAbs(rim ? t.mRimShadow : 0.f, 1e-3f);
    sig.addAbs(rim ? t.mRimTronMix : 0.f, 1e-3f);
    sig.addAbs(rim ? t.mRimTint : 0.f, 1e-3f);
    sig.addExact(rim && t.mRimDebugOnly);
    sig.addExact(rim && t.mRimIncludeAlpha);
    sig.addExact(static_cast<U64>(static_cast<U32>(rim ? t.mRimTronMode : 0)));
    sig.addExact(static_cast<U64>(static_cast<U32>(rim ? t.mRimTronColorSource : 0)));
    const S32 slots = std::min(t.mSlotCount, kMaxRimSlots);
    const S32 lights = std::min(t.mLightCount, kMaxRimLights);
    for (S32 i = 0; i < slots; ++i)
    {
        const bool on = rim && t.mSlotEnabled[i];
        sig.addExact(on);
        for (S32 j = 0; j < lights; ++j)
        {
            for (S32 c = 0; c < 4; ++c)
            {
                sig.addAbs(on ? t.mRimParams[i][j][c] : 0.f, 1e-3f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Live Probe ON-path H: lights come from the debounced light diff (published
// values), so flicker / moving facelights / rig transitions never move H while
// they run. Layout is fixed; see the brief section 5.2.
// ---------------------------------------------------------------------------
struct LiveToken
{
    LLUUID mId;
    U64 mStruct = 0;
    U64 mPubXform = 0;
    U64 mPubPhoto = 0;
};

inline LiveToken liveTokenOf(const LightEntry& e)
{
    LiveToken t;
    t.mId = e.mId;
    t.mStruct = e.mH[LS_STRUCT];
    t.mPubXform = e.mPubH[0];
    t.mPubPhoto = e.mPubH[1];
    return t;
}

// Does the Live capture select this entry? eligible, not ignored, and pinned,
// a spot, or within max_dist of the origin measured from the PUBLISHED position.
// The distance uses the most inclusive form of the capture's metric (radius
// bonus of an active light always applied): a superset is the safe direction.
inline bool liveSelects(const LightEntry& e, const F32 origin[3], F32 max_dist,
                        bool pinned, bool ignored)
{
    if (!e.mEligible || ignored)
    {
        return false;
    }
    if (pinned || e.mSpot)
    {
        return true;
    }
    F32 d2 = 0.f;
    for (S32 i = 0; i < 3; ++i)
    {
        const F32 d = e.mPubPos[i] - origin[i];
        d2 += d * d;
    }
    const F32 dist = std::max(std::sqrt(d2) - 1.25f * e.mRadius, 0.f);
    return dist < max_dist;
}

struct LiveLightGlobals
{
    F32 mGlobalScale = 1.f;
    bool mGoboAniso = false;
    bool mAttached = true;
    bool mBdToggles = false;
    bool mBdOwn = true;
    bool mBdOthers = true;
    bool mBdWorld = true;
    bool mBdProjectors = true;
};

struct LiveHInput
{
    F32 mOrigin[3] = { 0.f, 0.f, 0.f };
    F32 mRadius = 0.f;
    F32 mAmbiance = 0.f;
    S32 mCube = -1;
    F32 mMoveM = 0.05f;
    LiveLightGlobals mGlobals;
    S32 mEffectiveCount = 256; // max(RenderLocalLightCount, pinned count)
    U64 mSceneSerial = 0;
};

// `tokens` is sorted in place by id so the H is independent of selection order.
inline void buildLiveOnSignature(ALCineLiveProbeRefresh::Signature& sig,
                                 const LiveHInput& in, std::vector<LiveToken>& tokens,
                                 const EnvSample& env)
{
    // 1. Probe (the origin stays raw: the Live Probe follows its subject).
    sig.addAbs3(in.mOrigin, in.mMoveM);
    sig.addAbs(in.mRadius, 0.02f);
    sig.addAbs(in.mAmbiance, 0.01f);
    sig.addExact(static_cast<U64>(static_cast<U32>(in.mCube)));

    // 2. Light toggles / scale, then the tokens.
    sig.addAbs(in.mGlobals.mGlobalScale, 1e-3f);
    sig.addExact(in.mGlobals.mGoboAniso);
    sig.addExact(in.mGlobals.mAttached);
    sig.addExact(in.mGlobals.mBdToggles);
    sig.addExact(in.mGlobals.mBdOwn);
    sig.addExact(in.mGlobals.mBdOthers);
    sig.addExact(in.mGlobals.mBdWorld);
    sig.addExact(in.mGlobals.mBdProjectors);
    if (in.mEffectiveCount < 1)
    {
        sig.addExact(static_cast<U64>(0));
    }
    else
    {
        std::sort(tokens.begin(), tokens.end(),
                  [](const LiveToken& a, const LiveToken& b) { return a.mId < b.mId; });
        for (const LiveToken& t : tokens)
        {
            sig.addExact(t.mId);
            sig.addExact(t.mStruct);
            sig.addExact(t.mPubXform);
            sig.addExact(t.mPubPhoto);
        }
        sig.addExact(static_cast<U64>(tokens.size()));
    }

    // 3. Environment (HEAD tolerances, blend factors verbatim).
    appendEnvironmentFields(sig, env, 0.1f, false);

    // 4. Scene serial.
    sig.addExact(in.mSceneSerial);
}

// ---------------------------------------------------------------------------
// Texture notification rule (H6 / DS): quantised so camera-driven mip churn
// beyond what a probe can resolve never dirties probes, while a real arrival or
// a re-sharpen after a VRAM blur does.
// ---------------------------------------------------------------------------
// Largest k with (dim >> k) >= probe_res (0 when it cannot be satisfied).
inline S32 probeDiscardFloor(S32 dim, U32 probe_res)
{
    if (dim <= 0 || probe_res == 0)
    {
        return 0;
    }
    S32 k = 0;
    while (k < 30 && (static_cast<U32>(dim) >> (k + 1)) >= probe_res)
    {
        ++k;
    }
    return k;
}

struct TexNote
{
    S8 mNotedDiscard = -1;
    bool mBlurred = false;
    U64 mBlurStamp = 0;
};

struct TexVerdict
{
    bool mNotify = false;
    U64 mMinFaceOp = 0; // 0 = unconditional
};

// postCreateTexture: `d` = getDiscardLevel(), `dp` = probeDiscardFloor.
inline TexVerdict texArrival(TexNote& n, S32 d, S32 dp)
{
    TexVerdict v;
    if (d < 0)
    {
        return v;
    }
    const S32 noted = static_cast<S32>(n.mNotedDiscard);
    const bool uncond = noted < 0 || (d < noted && noted > dp); // (a) first, (b) coarse refinement
    const bool resharpen = n.mBlurred && d <= dp;                // (c)
    if (uncond)
    {
        v.mNotify = true;
    }
    else if (resharpen)
    {
        v.mNotify = true;
        v.mMinFaceOp = n.mBlurStamp;
    }
    if (resharpen)
    {
        n.mBlurred = false;
    }
    n.mNotedDiscard = static_cast<S8>(std::min(noted < 0 ? d : std::min(noted, d), 127));
    return v;
}

// After a VRAM downscale. The FIRST outstanding blur stamp is kept.
inline void texDownscale(TexNote& n, S32 new_discard, S32 dp, U64 stamp_op)
{
    if (new_discard > dp && static_cast<S32>(n.mNotedDiscard) <= dp && !n.mBlurred)
    {
        n.mBlurred = true;
        n.mBlurStamp = stamp_op;
    }
}

// DS fast check: would texDownscale() start a blur? (lets the caller take a
// stamp op only when one is recorded)
inline bool texDownscaleStartsBlur(const TexNote& n, S32 new_discard, S32 dp)
{
    return new_discard > dp && static_cast<S32>(n.mNotedDiscard) <= dp && !n.mBlurred;
}

// A face captured at `face_op` saw the blurred texture iff it rendered after
// the blur stamp (both are values of the same monotonic op counter).
inline bool faceExposed(U64 face_op, U64 blur_stamp)
{
    return face_op > blur_stamp;
}

// ---------------------------------------------------------------------------
// Window statistics and verdict ([ProbeSched] log).
// ---------------------------------------------------------------------------
struct SchedFrame
{
    U32 mOrdFaces = 0;
    U32 mRtFaces = 0;
    U32 mSlicedFaces = 0;
    bool mEarlyReturn = false;
};

// An implementation-bug detector: structurally unreachable.
inline bool frameOverBudget(const SchedFrame& f, S32 sliced_n)
{
    return f.mOrdFaces > 1 || f.mRtFaces > 6 ||
           (f.mSlicedFaces > 0 && f.mSlicedFaces > static_cast<U32>(std::max(sliced_n, 1)));
}

enum class Verdict : U8
{
    OK,
    UNSETTLED,
    STARVED,
    OVER_BUDGET,
    OFF_BASELINE,
    PAUSED
};

struct VerdictInput
{
    bool mOn = true;
    bool mOverBudget = false;
    bool mPaused = false;
    bool mUnsettled = false;
    U32 mUnsettledId = 0;
    U16 mUnsettledReasons = 0;
    U32 mStarvedCount = 0;
    F64 mStarvedWorst = 0.0;
};

// Priority: OVER BUDGET > PAUSED > UNSETTLED > STARVED > OK (OFF-BASELINE when
// the scheduler is off).
inline Verdict verdict(const VerdictInput& in)
{
    if (!in.mOn)
    {
        return Verdict::OFF_BASELINE;
    }
    if (in.mOverBudget)
    {
        return Verdict::OVER_BUDGET;
    }
    if (in.mPaused)
    {
        return Verdict::PAUSED;
    }
    if (in.mUnsettled)
    {
        return Verdict::UNSETTLED;
    }
    if (in.mStarvedCount > 0)
    {
        return Verdict::STARVED;
    }
    return Verdict::OK;
}

inline std::string verdictText(const VerdictInput& in)
{
    switch (verdict(in))
    {
        case Verdict::OK:           return "OK";
        case Verdict::UNSETTLED:
            return "UNSETTLED id=" + std::to_string(in.mUnsettledId) + " reasons=" +
                   reasonLetters(in.mUnsettledReasons);
        case Verdict::STARVED:
            return "STARVED " + std::to_string(in.mStarvedCount) + " " +
                   std::to_string(static_cast<S32>(in.mStarvedWorst + 0.5)) + "s";
        case Verdict::OVER_BUDGET:  return "OVER BUDGET";
        case Verdict::OFF_BASELINE: return "OFF-BASELINE";
        case Verdict::PAUSED:       return "PAUSED";
    }
    return "OK";
}

// A probe is STARVED when it has been dirty for longer than this.
constexpr F64 kStarveSec = 5.0;

// ---------------------------------------------------------------------------
// Settings clamps. MaxAge 0 is not allowed (no permanent safety disable).
// ---------------------------------------------------------------------------
inline F32 clampFinite(F32 v, F32 lo, F32 hi, F32 fallback)
{
    if (!std::isfinite(v))
    {
        v = fallback;
    }
    return std::min(std::max(v, lo), hi);
}

inline Policy policyFromSettings(F32 min_interval, F32 max_age)
{
    Policy p;
    p.mMinInterval = clampFinite(min_interval, 0.f, 10.f, 1.f);
    p.mMaxAge = clampFinite(max_age, 10.f, 600.f, 60.f);
    return p;
}

// ---------------------------------------------------------------------------
// Discrete notes: coalesced per key within one frame (bounds union, reason and
// class OR). They never enter the motion debounce, so a moving key can never
// delay a texture / content / membership change.
// ---------------------------------------------------------------------------
class DiscreteNotes
{
public:
    explicit DiscreteNotes(U32 cap = 4096) : mCap(cap) {}

    // Returns false when the note could not be stored (the cap overflowed).
    // `min_face_op` != 0 restricts the event to probes that rendered a face
    // after that op (a re-sharpened texture); coalescing keeps the most
    // inclusive value (0 = unconditional wins).
    bool note(const void* key, const F32 mn[3], const F32 mx[3], U16 reason, U8 cls,
              U64 serial, U64 min_face_op = 0, bool spot = false)
    {
        auto it = mIndex.find(key);
        if (it != mIndex.end())
        {
            Event& e = mEvents[it->second];
            boxUnion(e.mMin, e.mMax, mn, mx);
            e.mReason = static_cast<U16>(e.mReason | reason);
            e.mClass = static_cast<U8>(e.mClass | cls);
            e.mSpot = e.mSpot || spot;
            if (e.mMinFaceOp != 0)
            {
                e.mMinFaceOp = (min_face_op == 0) ? 0 : std::min(e.mMinFaceOp, min_face_op);
            }
            return true;
        }
        if (mEvents.size() >= static_cast<size_t>(mCap))
        {
            mOverflow = true;
            return false;
        }
        mIndex.emplace(key, static_cast<U32>(mEvents.size()));
        Event ev = makeEvent(mn, mx, reason, cls, serial);
        ev.mMinFaceOp = min_face_op;
        ev.mSpot = spot;
        mEvents.push_back(ev);
        return true;
    }

    // Move the frame's events out; reports and clears the overflow flag.
    bool take(std::vector<Event>& out)
    {
        for (const Event& e : mEvents)
        {
            out.push_back(e);
        }
        const bool overflow = mOverflow;
        clear();
        return overflow;
    }

    void markOverflow() { mOverflow = true; }

    void shift(const F32 off[3])
    {
        for (Event& e : mEvents)
        {
            boxOffset(e.mMin, e.mMax, off);
        }
    }

    void clear()
    {
        mEvents.clear();
        mIndex.clear();
        mOverflow = false;
    }

    size_t size() const { return mEvents.size(); }

private:
    U32 mCap;
    std::vector<Event> mEvents;
    std::unordered_map<const void*, U32> mIndex;
    bool mOverflow = false;
};

// ---------------------------------------------------------------------------
// VO-cache provenance. An object the region's object cache CULLED (the camera
// turned) and later re-created from the cache is "cache-born": not a scene change.
// Only an UNCHANGED re-creation may be cache-born: any authoritative change (a
// full / compressed update with a new CRC, a replaced cache entry, a cache miss
// followed by a full update, a server update, a real kill) clears BOTH the culled
// marker and the cache-born state of that id, so a changed object is never hidden
// and a stale culled marker can never suppress a later genuine removal.
// ---------------------------------------------------------------------------
class CacheProvenance
{
public:
    explicit CacheProvenance(size_t cap = 16384) : mCap(cap) {}

    void noteCulled(U64 tag)
    {
        if (mCulled.size() >= mCap)
        {
            mCulled.clear();
        }
        mCulled.insert(tag);
    }
    // Re-created from the cache: cache-born iff it was culled (and not changed since).
    bool noteCreated(U64 tag)
    {
        if (mCulled.erase(tag) == 0)
        {
            return false;
        }
        if (mBorn.size() >= mCap)
        {
            mBorn.clear();
        }
        mBorn.insert(tag);
        return true;
    }
    // Authoritative change or real kill.
    void noteAuthoritative(U64 tag)
    {
        mCulled.erase(tag);
        mBorn.erase(tag);
    }
    bool isBorn(U64 tag) const { return !mBorn.empty() && mBorn.count(tag) > 0; }
    bool wasCulled(U64 tag) const { return !mCulled.empty() && mCulled.count(tag) > 0; }
    bool bornEmpty() const { return mBorn.empty(); }
    bool culledEmpty() const { return mCulled.empty(); }
    void clear()
    {
        mCulled.clear();
        mBorn.clear();
    }

private:
    size_t mCap;
    std::unordered_set<U64> mCulled;
    std::unordered_set<U64> mBorn;
};

} // namespace ALProbeSched

#endif // AL_PROBE_SCHEDULE_H
