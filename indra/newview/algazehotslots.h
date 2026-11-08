/** Scene-owned hot gaze setup. No live firing state is serialised here. */
#ifndef AL_GAZE_HOT_SLOTS_H
#define AL_GAZE_HOT_SLOTS_H
#include "llactormover.h"
#include "llsdutil_math.h"
#include <array>

namespace ALGazeHot
{
struct Slot
{
    LLActorMover::GazeTarget mTarget;
    LLUUID mWho;
    std::string mGroup, mTargetName;
    F32 mSnap = .12f, mHead = 1.f, mEyes = 1.f, mTorso = .2f;
    F32 mDwell = 2.f, mReturn = 1.2f;
    bool mHold = false, mStay = true, mHasTarget = false;
};
using Slots = std::array<Slot, 4>;

inline bool rewrite(Slots& slots, const LLUUID& old_id, const LLUUID& new_id)
{
    bool changed = false;
    for (auto& slot : slots)
    {
        if (slot.mWho == old_id) { slot.mWho = new_id; changed = true; }
        if (slot.mTarget.mCastRef == old_id || slot.mTarget.mObjectRef == old_id)
        {
            if (slot.mTarget.mCastRef == old_id) slot.mTarget.mCastRef = new_id;
            if (slot.mTarget.mObjectRef == old_id) slot.mTarget.mObjectRef = new_id;
            if (new_id.isNull()) { slot.mHasTarget = false; slot.mTargetName.clear(); }
            changed = true;
        }
    }
    return changed;
}

template<class Persistable>
LLSD writeSlots(const Slots& slots, Persistable persistable)
{
    LLSD out = LLSD::emptyArray();
    for (auto slot : slots)
    {
        if (!persistable(slot.mWho)) slot.mWho.setNull();
        if (!persistable(slot.mTarget.mCastRef) || !persistable(slot.mTarget.mObjectRef))
        { slot.mTarget = {}; slot.mHasTarget = false; slot.mTargetName.clear(); }
        LLSD s;
        s["who"] = slot.mWho; s["group"] = slot.mGroup;
        s["target_name"] = slot.mTargetName; s["has_target"] = slot.mHasTarget;
        s["mode"] = static_cast<S32>(slot.mTarget.mMode);
        s["cast_ref"] = slot.mTarget.mCastRef; s["object_ref"] = slot.mTarget.mObjectRef;
        s["point"] = ll_sd_from_vector3d(slot.mTarget.mFixedPoint);
        s["picked_spot"] = slot.mTarget.mPickedSpot;
        s["object_offset"] = ll_sd_from_vector3(slot.mTarget.mObjectOffset);
        s["object_joint"] = slot.mTarget.mObjectJoint;
        s["point_nudge"] = ll_sd_from_vector3(slot.mTarget.mPointNudge);
        s["snap"] = slot.mSnap; s["head"] = slot.mHead; s["eyes"] = slot.mEyes;
        s["torso"] = slot.mTorso; s["dwell"] = slot.mDwell; s["return"] = slot.mReturn;
        s["hold"] = slot.mHold; s["stay"] = slot.mStay;
        s["return_after"] = !slot.mStay; // Keep stay as a compatibility mirror for older builds.
        out.append(s);
    }
    return out;
}
inline F32 number(const LLSD& s, const char* key, F32 fallback, F32 maximum)
{
    const F64 v = s.has(key) ? s[key].asReal() : fallback;
    return std::isfinite(v) ? static_cast<F32>(llclamp(v, 0.0, static_cast<F64>(maximum))) : fallback;
}
inline Slots readSlots(const LLSD& data)
{
    Slots slots;
    if (!data.isArray()) return slots;
    for (S32 i = 0; i < llmin(data.size(), 4); ++i)
    {
        const LLSD& s = data[i];
        if (!s.isMap()) continue;
        Slot& slot = slots[i];
        slot.mWho = s["who"].asUUID(); slot.mGroup = s["group"].asString();
        slot.mTargetName = s["target_name"].asString(); slot.mHasTarget = s["has_target"].asBoolean();
        const S32 mode = s["mode"].asInteger();
        if (mode < LLActorMover::GazeTarget::CAMERA || mode > LLActorMover::GazeTarget::OBJECT)
            slot.mHasTarget = false;
        else slot.mTarget.mMode = static_cast<LLActorMover::GazeTarget::EMode>(mode);
        slot.mTarget.mCastRef = s["cast_ref"].asUUID(); slot.mTarget.mObjectRef = s["object_ref"].asUUID();
        slot.mTarget.mFixedPoint = ll_vector3d_from_sd(s["point"]);
        slot.mTarget.mPickedSpot = s["picked_spot"].asBoolean();
        slot.mTarget.mObjectOffset = ll_vector3_from_sd(s["object_offset"]);
        slot.mTarget.mObjectJoint = s["object_joint"].asString();
        slot.mTarget.mPointNudge = ll_vector3_from_sd(s["point_nudge"]);
        if (!slot.mTarget.mObjectOffset.isFinite() || !slot.mTarget.mPointNudge.isFinite() ||
            !slot.mTarget.mFixedPoint.isFinite()) slot.mHasTarget = false;
        slot.mSnap = number(s, "snap", .12f, 1.5f);
        slot.mHead = number(s, "head", 1.f, 1.f); slot.mEyes = number(s, "eyes", 1.f, 1.f);
        slot.mTorso = number(s, "torso", .2f, 1.f); slot.mDwell = number(s, "dwell", 2.f, 3600.f);
        slot.mReturn = number(s, "return", 1.2f, 60.f);
        slot.mHold = s["hold"].asBoolean();
        // Legacy slots used automatic return by default; require an explicit new opt-in.
        slot.mStay = !s.has("return_after") || !s["return_after"].asBoolean();
    }
    return slots;
}
}
#endif
