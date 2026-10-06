/**
 * @file llsliderwheel.h
 * @brief [SliderWheel] Viewer-wide slider mouse wheel: scroll-through guard, wheel settle
 *        registry, and the callback-lifetime helpers.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Design: doc/SLIDER_WHEEL_GLOBAL_DESIGN.md (v7). The UI-independent logic (steps, resolution
 * table, guard and settle state machines) lives in llsliderwheelcore.h.
 *
 * STANDING RULE (CLAUDE.md): any scroller that moves content without going through
 * LLScrollbar::scrollByWheel must call LLSliderWheel::noteScroll(this) when it really moves,
 * otherwise a slider inside it can steal the wheel mid-scroll.
 */

#ifndef LL_LLSLIDERWHEEL_H
#define LL_LLSLIDERWHEEL_H

#include "llsliderwheelcore.h"

#include "llhandle.h"
#include "llview.h"

class LLFloater;
class LLSlider;

namespace LLSliderWheel
{
// ---- scroll-through guard (design 1) ------------------------------------------------------

// A scroller really moved its content from a wheel event: arm the guard for `owner`
// (the scrollbar passes its parent). Null is ignored.
void noteScroll(LLView* owner);

// A guarded wheel event was redirected / swallowed: extend the guard.
void keepAlive();

// True when the guard is fresh (< GUARD_SEC), its owner is alive, and the owner is `v` or an
// ancestor of `v`.
bool guarded(const LLView* v);

// WheelGuard's old hit test, verbatim: childFromPoint(x, y, true) under `container`, walking up
// to `container`, returning the LLSlider (or the inner slider of an LLSliderCtrl) found.
// Finds disabled sliders too (dispatch skips them).
LLSlider* sliderAt(LLView* container, S32 x, S32 y);

// ---- lifetime (design 4.2b) ---------------------------------------------------------------

// Counter of alive() calls: tests assert it stays flat during idle frames.
inline U32& aliveCallCounter()
{
    static U32 sCount = 0;
    return sCount;
}

// Re-resolve a handle after a call that can run foreign code. Returns null if the object is
// gone OR it or ANY ancestor was die()d (views are LLMorticians, so die() leaves a valid but dead
// object: this walk is memory-safe; a direct delete kills the handle first).
template <class T>
T* alive(const LLHandle<T>& h)
{
    ++aliveCallCounter();
    T* p = h.get();
    if (!p)
    {
        return nullptr;
    }
    for (LLView* v = p; v; v = v->getParent())
    {
        if (v->isDead())
        {
            return nullptr;
        }
    }
    return p;
}

// ---- settings / clock ---------------------------------------------------------------------

// UISliderWheelAdjust (default on).
bool switchOn();
// Monotonic seconds (LLTimer::getTotalSeconds).
F64  now();

// ---- wheel settle (design 4.2) ------------------------------------------------------------

// Upsert the slider's single pending entry (deadline = absolute seconds); starts the shared
// timer if needed. No-op after cancelAllForQuit().
void scheduleSettle(const LLHandle<LLSlider>& h, U32 gen, F64 deadline);

// Close flush (LLFloater::closeFloater, immediately before onClose): fires the pending settles of
// every live slider that is `root` or a descendant of it. `serial` is the floater's lifecycle
// serial taken by the caller; if the floater is deleted, dies, or its serial changes (nested
// close / reopen) the remaining entries go back to the tick unfired. With app_quitting (or any
// quit flag) the entries are cancelled, never fired.
void settleForClose(LLHandle<LLFloater> root, U32 serial, bool app_quitting);

// Quit: drop every entry (bumping each slider's generation), latch the quitting flag so nothing
// schedules or fires afterwards. Called immediately before both closeAllChildren(true) sites.
void cancelAllForQuit();

bool isQuitting();
} // namespace LLSliderWheel

#endif // LL_LLSLIDERWHEEL_H
