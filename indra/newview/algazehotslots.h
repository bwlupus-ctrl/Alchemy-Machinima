/** Scene-owned hot gaze setup. No live firing state is serialised here. */
#ifndef AL_GAZE_HOT_SLOTS_H
#define AL_GAZE_HOT_SLOTS_H
#include "llactormover.h"
#include "llsdutil_math.h"
#include <array>
#include <algorithm>
#include <map>

namespace ALGazeHot
{
struct Slot
{
    LLActorMover::GazeTarget mTarget;
    LLUUID mWho;
    std::string mGroup, mTargetName;
    F32 mSnap = .12f, mHead = 1.f, mEyes = 1.f, mTorso = .2f;
    F32 mDwell = 2.f, mReturn = 1.2f;
    bool mAuto = true, mHold = false, mStay = true, mHasTarget = false;
};
using Slots = std::array<Slot, 4>;

// Scene reloads must re-arm even an identical setup. Runtime state is never saved.
inline U64& loadGeneration() { static U64 generation = 0; return generation; }

inline bool sameSetup(const Slot& a, const Slot& b)
{
    // Target names are asynchronous display metadata, not a new look.
    const auto& x = a.mTarget; const auto& y = b.mTarget;
    return a.mAuto == b.mAuto && a.mHold == b.mHold && a.mStay == b.mStay &&
        a.mHasTarget == b.mHasTarget && a.mWho == b.mWho && a.mGroup == b.mGroup &&
        a.mSnap == b.mSnap && a.mHead == b.mHead && a.mEyes == b.mEyes &&
        a.mTorso == b.mTorso && a.mDwell == b.mDwell && a.mReturn == b.mReturn &&
        x.mMode == y.mMode && x.mCastRef == y.mCastRef && x.mObjectRef == y.mObjectRef &&
        x.mFixedPoint == y.mFixedPoint && x.mPickedSpot == y.mPickedSpot &&
        x.mObjectOffset == y.mObjectOffset && x.mObjectJoint == y.mObjectJoint &&
        x.mPointNudge == y.mPointNudge && x.mIntensityOverride == y.mIntensityOverride;
}

// Manual input records intent only; dispatch stays in LLDirectorCast.
struct ManualControl { U64 generation = 0; bool active = false; };
inline std::array<ManualControl, 4>& manualControls() { static std::array<ManualControl, 4> value{}; return value; }
inline bool manualMode(S32 index)
{
    const auto& control = manualControls()[index];
    return control.active && control.generation == loadGeneration();
}
inline U64& manualRevision() { static U64 revision = 0; return revision; }
inline void noteManual(S32 index) { manualControls()[index] = {loadGeneration(), true}; ++manualRevision(); }

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

// Latches record readiness/intent, never a copy of the mover's live envelopes.
class AutoSlots
{
public:
    AutoSlots() { mGeneration.fill(loadGeneration()); }
    void edited(S32 index) { mDirty[index] = true; manualControls()[index].active = false; }
    void renameGroup(const std::string& old_name, const std::string& new_name)
    {
        for (auto& slot : mPrevious) if (slot.mGroup == old_name) slot.mGroup = new_name;
    }
    void rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id)
    {
        rewrite(mPrevious, old_id, new_id);
        for (auto& latches : mFired)
        {
            const auto it = latches.find(old_id);
            if (it == latches.end()) continue;
            if (new_id.notNull()) latches[new_id] = it->second;
            latches.erase(it);
        }
    }
    template<class Who, class Fire, class Query>
    void update(const Slots& slots, U64 generation, F64 now, Who who, Fire fire, Query query)
    {
        using State = LLActorMover::HotGazeSlotState;
        for (S32 i = 0; i < 4; ++i)
        {
            const auto& slot = slots[i];
            const bool edited = mDirty[i] || !sameSetup(slot, mPrevious[i]);
            if (generation != mGeneration[i] || edited)
            {
                // The public setter releases edits; scene load clears the mover itself.
                // Never release here: a manual Fire may have preceded this first load poll.
                mFired[i].clear();
                mDirty[i] = false; mPrevious[i] = slot; mGeneration[i] = generation;
            }
            if (!slot.mAuto || !slot.mHasTarget) continue;
            auto members = who(slot);
            std::sort(members.begin(), members.end());
            members.erase(std::unique(members.begin(), members.end()), members.end());
            auto& latches = mFired[i];
            for (auto it = latches.begin(); it != latches.end();)
                if (!std::binary_search(members.begin(), members.end(), it->first)) it = latches.erase(it);
                else ++it;
            for (const auto& actor : members)
            {
                if (actor.isNull()) continue;
                const auto info = query(actor, i);
                const auto state = info.state;
                auto& latch = latches[actor];
                const bool same = latch.start == info.start && latch.serial == info.serial;
                // The setter retired this envelope. A failed replacement must never adopt it
                // on a later poll, even if its entire return fits between polls.
                if (edited && (state == State::RETURNING || state == State::FINISHED))
                    latch = {info.start, info.serial, 0.0, true};
                const bool live = state == State::ACTIVE || state == State::RETURNING;
                if (live && !(edited && state == State::RETURNING) && (!latch.pending || !same))
                {
                    if (!same || latch.pending)
                        latch = {info.start, info.serial, info.start + slot.mSnap + slot.mDwell + slot.mReturn, false};
                    continue;
                }
                // A press takes over the entire slot, including unresolved recipients.
                if (manualMode(i)) continue;
                if (state == State::ACTIVE) continue;
                // Natural completion does not require a RETURNING sample. Explicit early
                // releases/erases recover; a vanished envelope is complete only at its end.
                constexpr F64 tolerance = .001;
                // An older finished snapshot is not evidence about the latched look:
                // its running entry was erased, so use that look's expected end.
                const bool stale = state == State::FINISHED &&
                    (info.start < latch.start || (info.start == latch.start && info.serial < latch.serial));
                if (!latch.pending && !slot.mStay &&
                    ((same && state == State::FINISHED && (info.release < 0.0 || info.release + slot.mReturn + tolerance >= latch.end)) ||
                     ((state == State::NONE || stale) && now + tolerance >= latch.end))) continue;
                bool owned = false;
                for (S32 other = 0; other < 4; ++other)
                    if (other != i)
                    {
                        const auto owner = query(actor, other).state;
                        if (owner == State::ACTIVE || owner == State::RETURNING) { owned = true; break; }
                    }
                if (owned) { latch.pending = true; continue; }
                fire(actor, i, slot);
                const auto fired = query(actor, i);
                // Start plus the existing per-envelope paint generation distinguishes even
                // two fires in one presentation frame. Suppression leaves the old identity.
                const bool replacement = fired.state != State::NONE &&
                    (info.state == State::NONE || fired.start != info.start || fired.serial != info.serial);
                if (replacement)
                    latch = {fired.start, fired.serial, fired.start + slot.mSnap + slot.mDwell + slot.mReturn, false};
                else
                    latch = {info.start, info.serial, 0.0, true};
            }
        }
    }
private:
    struct Latch { F64 start = -1.0; U32 serial = 0; F64 end = 0.0; bool pending = true; };
    Slots mPrevious;
    std::array<std::map<LLUUID, Latch>, 4> mFired;
    std::array<bool, 4> mDirty{};
    std::array<U64, 4> mGeneration{};
};

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
        s["auto"] = slot.mAuto;
        s["hold"] = !slot.mAuto && slot.mHold; s["stay"] = slot.mStay;
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
        slot.mAuto = s.has("auto") && s["auto"].asBoolean();
        slot.mHold = !slot.mAuto && s["hold"].asBoolean();
        // Legacy slots used automatic return by default; require an explicit new opt-in.
        slot.mStay = !s.has("return_after") || !s["return_after"].asBoolean();
    }
    ++loadGeneration();
    return slots;
}
}
#endif
