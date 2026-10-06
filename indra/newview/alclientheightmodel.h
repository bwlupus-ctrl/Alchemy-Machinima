/**
 * @file alclientheightmodel.h
 * @brief [ClientHeight] Pure model for the client-side avatar height (a local,
 *        render-only vertical offset): the offset store, the per-avatar
 *        resolve/cache decision and the helpers the root writers share.
 *
 * No GL, no viewer globals, no LLVOAvatar: LLUUID + math only, so every rule
 * the viewer relies on can be unit tested. See doc/CLIENT_AVATAR_HEIGHT_DESIGN.md.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_CLIENT_HEIGHT_MODEL_H
#define AL_CLIENT_HEIGHT_MODEL_H

#include "stdtypes.h"

#include "llmath.h"
#include "llquaternion.h"
#include "lluuid.h"
#include "v3math.h"

#include <cmath>
#include <map>

namespace ALClientHeightModel
{
constexpr F32 MAX_OFFSET   = 2.f;       // metres, +/-
constexpr F32 ZERO_EPSILON = 5.0e-4f;   // below half a millimetre is "no offset"

// Finite-only, clamped to +/-MAX_OFFSET, near-zero snapped to exactly 0.
// Returns false (out untouched) for NaN / Inf.
inline bool sanitize(F32 z, F32& out)
{
    if (!std::isfinite(z))
    {
        return false;
    }
    F32 c = z;
    if (c > MAX_OFFSET)  c = MAX_OFFSET;
    if (c < -MAX_OFFSET) c = -MAX_OFFSET;
    if (std::fabs(c) < ZERO_EPSILON)
    {
        c = 0.f;
    }
    out = c;
    return true;
}

// Session-only map key -> metres. A set to 0 erases the entry. The revision is
// bumped ONLY on a real change, so readers can compare one integer per frame.
class Store
{
public:
    // True on a real change. Null keys and non-finite values are rejected.
    bool set(const LLUUID& key, F32 z)
    {
        F32 s = 0.f;
        if (key.isNull() || !sanitize(z, s))
        {
            return false;
        }
        const auto it = mEntries.find(key);
        if (s == 0.f)
        {
            if (it == mEntries.end())
            {
                return false;
            }
            mEntries.erase(it);
            ++mRevision;
            return true;
        }
        if (it != mEntries.end())
        {
            if (it->second == s)
            {
                return false;
            }
            it->second = s;
        }
        else
        {
            mEntries.emplace(key, s);
        }
        ++mRevision;
        return true;
    }

    F32 get(const LLUUID& key) const
    {
        if (mEntries.empty())
        {
            return 0.f;
        }
        const auto it = mEntries.find(key);
        return it != mEntries.end() ? it->second : 0.f;
    }

    // True when an entry was removed. An absent key does not bump the revision.
    bool reset(const LLUUID& key) { return set(key, 0.f); }

    // Empties the store. Bumps the revision once, and only when it was non-empty.
    bool resetAll()
    {
        if (mEntries.empty())
        {
            return false;
        }
        mEntries.clear();
        ++mRevision;
        return true;
    }

    // The disconnect path: offsets must not leak across accounts.
    bool clearSession() { return resetAll(); }

    bool   empty() const { return mEntries.empty(); }
    size_t size() const { return mEntries.size(); }
    U64    revision() const { return mRevision; }
    const std::map<LLUUID, F32>& entries() const { return mEntries; }

private:
    std::map<LLUUID, F32> mEntries;
    U64                   mRevision = 1;
};

// What the cached per-avatar value was resolved against.
struct Stamp
{
    bool   mValid = false;
    U64    mStoreRev = 0;
    U64    mGhostInstRev = 0;
    LLUUID mWearerKey;
};

// The pure re-resolve decision. A cached value is trusted only while NONE of
// these changed:
//  (a) the store revision;
//  (b) the Ghost Studio instance revision (pass 0 for non-ghosts);
//  (b') the wearer key of a worn control avatar (attach / detach / re-attach);
//  (c) the cached key is null: never treated as final;
//  (d) the live key hint (stamped ghost key, or the animesh linkset root, which
//      arrives late) is non-null and differs from the cached key.
inline bool needsResolve(const Stamp& stamp, U64 store_rev, U64 ghost_inst_rev,
                         const LLUUID& wearer_key, const LLUUID& cached_key,
                         const LLUUID& live_key_hint)
{
    if (!stamp.mValid)                              return true;
    if (stamp.mStoreRev != store_rev)               return true;
    if (stamp.mGhostInstRev != ghost_inst_rev)      return true;
    if (stamp.mWearerKey != wearer_key)             return true;
    if (cached_key.isNull())                        return true;
    if (live_key_hint.notNull() && live_key_hint != cached_key) return true;
    return false;
}

// Effective offset: own, plus the wearer's when the control avatar is worn.
// For every other avatar effective == own (pass worn = false).
inline F32 effectiveOffset(F32 own, bool worn, F32 wearer)
{
    return worn ? own + wearer : own;
}

// Per-avatar cache. Reads the wearer's value straight from the STORE, by key,
// never from the wearer's own cache, so the result does not depend on avatar
// update order.
class Cache
{
public:
    F32 own() const { return mOwn; }
    F32 effective() const { return mEffective; }
    const LLUUID& key() const { return mKey; }
    const Stamp& stamp() const { return mStamp; }

    // Returns true when the OWN or the EFFECTIVE value changed (a "forced update"
    // for the caller). resolve_key() is invoked only when a re-resolve is needed
    // and returns the avatar's store key (it may be null: not yet known).
    template <class KeyFn>
    bool refresh(const Store& store, U64 ghost_inst_rev, const LLUUID& live_key_hint,
                 bool worn, const LLUUID& wearer_key, KeyFn&& resolve_key)
    {
        if (store.empty())
        {
            // Fast path: one branch. Invalidate so a later non-empty store re-resolves.
            mStamp.mValid = false;
            return assign(0.f, 0.f);
        }
        const LLUUID wearer = worn ? wearer_key : LLUUID::null;
        if (!needsResolve(mStamp, store.revision(), ghost_inst_rev, wearer, mKey, live_key_hint))
        {
            return false;
        }
        mKey = resolve_key();
        const F32 own = mKey.notNull() ? store.get(mKey) : 0.f;
        const F32 wearer_z = wearer.notNull() ? store.get(wearer) : 0.f;
        mStamp.mValid = true;
        mStamp.mStoreRev = store.revision();
        mStamp.mGhostInstRev = ghost_inst_rev;
        mStamp.mWearerKey = wearer;
        return assign(own, effectiveOffset(own, worn, wearer_z));
    }

    void clear()
    {
        mOwn = mEffective = 0.f;
        mKey.setNull();
        mStamp = Stamp();
    }

private:
    bool assign(F32 own, F32 eff)
    {
        const bool changed = own != mOwn || eff != mEffective;
        mOwn = own;
        mEffective = eff;
        return changed;
    }

    F32    mOwn = 0.f;
    F32    mEffective = 0.f;
    LLUUID mKey;
    Stamp  mStamp;
};

// A changed offset must force one full update even on a LOD-throttled avatar,
// and must re-assert the impostor flag AFTER computeNeedsUpdate() (the control
// avatar override overwrites it with the wearer's value).
inline void applyForcedUpdate(bool changed, bool& needs_update, bool& needs_impostor_update)
{
    if (changed)
    {
        needs_update = true;
        needs_impostor_update = true;
    }
}

// Ground sit adds its hover/client lift AFTER the Actor Mover override. The mover already
// wrote base + effective offset, so the client term must be suppressed in that case (stock
// hover is unaffected): exactly ONE client term per frame.
inline F32 groundSitClientTerm(bool mover_wrote_root, F32 own_client_z)
{
    return mover_wrote_root ? 0.f : own_client_z;
}

// A WORLD-vertical lift expressed in a seat-local frame: (0,0,z) * ~seatWorldRot,
// so that rotating it by the seat's world rotation gives (0,0,z) again.
inline LLVector3 worldLiftInSeatSpace(F32 z, const LLQuaternion& seat_world_rot)
{
    return LLVector3(0.f, 0.f, z) * ~seat_world_rot;
}
} // namespace ALClientHeightModel

#endif // AL_CLIENT_HEIGHT_MODEL_H
