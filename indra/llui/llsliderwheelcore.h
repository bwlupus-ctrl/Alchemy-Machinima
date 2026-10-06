/**
 * @file llsliderwheelcore.h
 * @brief [SliderWheel] Pure (UI-independent) logic of the viewer-wide slider mouse wheel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Design: doc/SLIDER_WHEEL_GLOBAL_DESIGN.md (v7).
 *
 * Everything here is free of LLView / LLSlider / LLFloater so it can be unit tested by the
 * llui test harness (tests/llsliderwheelcore_test.cpp). The UI-bound layer (llsliderwheel.h)
 * instantiates these with LLHandle<LLView> / LLHandle<LLSlider>.
 */

#ifndef LL_LLSLIDERWHEELCORE_H
#define LL_LLSLIDERWHEELCORE_H

#include "stdtypes.h"

#include <utility>
#include <vector>

namespace LLSliderWheel
{
// A scroller stamped the guard this recently => a slider under it ignores the wheel (the panel
// keeps scrolling). Same value as the retired ALScrollFit::WHEEL_GUARD_SEC.
constexpr F64 GUARD_SEC  = 0.5;
// Quiet time after the last wheel notch before the single final "mouse-up" is synthesised.
constexpr F64 SETTLE_SEC = 0.3;
// Period of the shared settle timer (exists only while a settle is pending).
constexpr F32 TICK_SEC   = 0.05f;

// LLSlider wheel_adjust tri-state (design 2.1). UNSET = the param was not provided.
enum EWheelAdjust { WHEEL_UNSET, WHEEL_ON, WHEEL_OFF };

// Design 2.2 / 2.3. `switch_on` = the UISliderWheelAdjust setting.
struct Resolution
{
    bool participates;   // ON, or UNSET with the switch on: gates ALL new behaviour
    bool wheelEnabled;   // vertical-wheel dispatch (LLSliderCtrl forwarding)
    bool hwheelEnabled;  // !OFF
};
Resolution resolve(EWheelAdjust state, bool vertical, bool switch_on);

// Largest of {1,2,5} x 10^k that is <= x, minimum 1 (design 3).
F32 nice125(F32 x);

// Default wheel step of an UNSET horizontal slider: nice125(range / (100 * inc)) * inc.
// Always a multiple of `increment`.
F32 plainStep(F32 min_value, F32 max_value, F32 increment);

// Per-notch step for a participating slider WITHOUT wheel_fine_div (design 3).
//   legacy (explicit ON, or any vertical slider): increment, Ctrl/Alt = x10; Shift is ignored.
//   otherwise (UNSET horizontal): plainStep; Shift = increment (wins); Ctrl/Alt = 10 * plainStep.
F32 wheelStep(F32 min_value, F32 max_value, F32 increment, bool legacy, bool shift, bool ctrl_alt);

// Whether a wheel change may arm a settle (design 4.2 "Schedule").
bool shouldSchedule(bool has_mouse_up_signal, bool captured, bool quitting);

// Per-slider settle generation (design 4.2): every invalidation bumps gen and clears pending.
struct SettleState
{
    U32  gen     = 0;
    bool pending = false;

    void invalidate() { ++gen; pending = false; }
    void arm()        { pending = true; }
    // True exactly once per arm, and only for the generation that was armed.
    bool consume(U32 g)
    {
        if (!pending || g != gen) return false;
        pending = false;
        return true;
    }
};

// What the settle timer does with one registry entry (design 4.2 "Tick", step 2).
enum class TickAction { Drop, Defer, Wait, Fire };
TickAction tickDecision(bool alive, bool gen_matches, bool pending, bool captured, F64 now, F64 deadline);

// Owner (the stamped scroller) is `v` itself or one of its ancestors. A null owner never guards.
// `Node` needs `Node* getParent() const`.
template <class Node>
bool isSelfOrAncestor(const Node* owner, const Node* v)
{
    if (!owner) return false;
    for (const Node* n = v; n; n = n->getParent())
    {
        if (n == owner) return true;
    }
    return false;
}

// Scroll-through guard state (design 1.1), clock injected. `Handle` needs `Node* get() const`
// (null when the owner is gone) and a default constructor.
template <class Handle, class Node>
class GuardState
{
public:
    void note(const Handle& owner, F64 now) { mOwner = owner; mStamp = now; }
    void keepAlive(F64 now)                  { mStamp = now; }
    bool fresh(F64 now) const                { return (now - mStamp) < GUARD_SEC; }
    bool guarded(const Node* v, F64 now) const
    {
        return fresh(now) && isSelfOrAncestor<Node>(mOwner.get(), v);
    }
    void reset() { mOwner = Handle(); mStamp = -1.0e9; }

private:
    Handle mOwner;
    F64    mStamp = -1.0e9;
};

// Settle registry (design 4.2): at most ONE entry per slider handle. `Handle` needs operator==.
template <class Handle>
class SettleTable
{
public:
    struct Entry
    {
        Handle handle;
        U32    gen;
        F64    deadline;
    };

    // A later notch moves the deadline (and generation) of the slider's existing entry.
    void upsert(const Handle& h, U32 gen, F64 deadline)
    {
        for (Entry& e : mEntries)
        {
            if (e.handle == h)
            {
                e.gen = gen;
                e.deadline = deadline;
                return;
            }
        }
        mEntries.push_back(Entry{ h, gen, deadline });
    }

    // Re-insert an entry that was taken out. Merges with an entry added meanwhile (one per
    // slider, newest gen wins; on a tie the existing, newer-deadline entry stays).
    void insertMerge(const Entry& in)
    {
        for (Entry& e : mEntries)
        {
            if (e.handle == in.handle)
            {
                if (e.gen < in.gen) e = in;
                return;
            }
        }
        mEntries.push_back(in);
    }

    // Swap the whole registry out (the tick works on this copy, so callbacks that schedule
    // or cancel only ever touch the live table).
    std::vector<Entry> takeAll()
    {
        std::vector<Entry> out;
        out.swap(mEntries);
        return out;
    }

    // Remove and return every entry for which pred(entry) is true.
    template <class Pred>
    std::vector<Entry> extractIf(Pred pred)
    {
        std::vector<Entry> out;
        std::vector<Entry> keep;
        for (Entry& e : mEntries)
        {
            if (pred(e)) out.push_back(e);
            else         keep.push_back(e);
        }
        mEntries.swap(keep);
        return out;
    }

    bool   contains(const Handle& h) const
    {
        for (const Entry& e : mEntries)
        {
            if (e.handle == h) return true;
        }
        return false;
    }
    bool   empty() const { return mEntries.empty(); }
    size_t size() const  { return mEntries.size(); }
    void   clear()       { mEntries.clear(); }

private:
    std::vector<Entry> mEntries;
};
} // namespace LLSliderWheel

#endif // LL_LLSLIDERWHEELCORE_H
