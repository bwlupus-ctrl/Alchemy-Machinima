/**
 * @file aluirefresh.h
 * @brief Shared "no work in draw()" helpers for Alchemy-Machinima UI panels.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [GhostPanelPerf] Header-only. Rationale and evidence: doc/GHOST_PERF_AUDIT_FABLE.md;
 * the rule these helpers serve is "UI PANELS MUST NOT DO WORK IN draw()" in CLAUDE.md.
 *
 * What a panel's draw() may do (and only this):
 *   1. ask a gate (revision / clock / on-screen) whether anything is due, and
 *   2. perform diff-guarded writes (the *IfChanged helpers below).
 *
 * DIFF-GUARDED WRITES (templates, so they reach the most-derived widget type).
 * Each returns true when it actually wrote:
 *     setTextIfChanged(w, s, force)       LLTextBox / LLTextEditor / LLLineEditor
 *     setValueIfChanged(w, v, force)      any LLUICtrl (widget-aware comparison, below)
 *     setToolTipIfChanged(w, tip)         any LLView
 *     setEnabledIfChanged(w, b)           any LLView
 *     setVisibleIfChanged(w, b)           any LLView
 *     setMaxValueIfChanged(w, max)        LLSlider / LLSliderCtrl / any LLF32UICtrl
 *     setLabelIfChanged(button|check, s)  LLButton / LLCheckBoxCtrl
 *
 * FOCUS RULE (setTextIfChanged / setValueIfChanged): the write is SKIPPED, returning
 * false, when the control OR ANY DESCENDANT has keyboard focus (a spinner's inner
 * line editor, a slider ctrl's edit box, a combo's text entry) mouse capture (a
 * slider thumb drag; an open combo dropdown focuses its list): gFocusMgr.childHasKeyboardFocus / childHasMouseCapture, the
 * same guard alpanelcinelightrig.cpp syncFixtureControls uses. Because nothing was
 * written, a caller that caches "what the widget shows" must NOT update that cache
 * on a false return. `force = true` bypasses the guard, for callers that
 * intentionally resync (selection changed) and for check boxes / buttons, whose
 * "focus" after a click is meaningless to editing.
 *
 * WIDGET-AWARE COMPARISON (setValueIfChanged; auto-detected by dynamic_cast, callers
 * pass no epsilon). "Unchanged" means what the widget's own setValue(const LLSD&)
 * would leave it showing -- nothing is clamped or rounded where the widget does not:
 *   - LLSlider / LLSliderCtrl: clamp, then either the wheel_fine_div snap
 *     (snapToFineGrid) or increment rounding, exactly as llslider.cpp does;
 *   - LLSpinCtrl and other LLF32UICtrl: the RAW F32 (llspinctrl.cpp stores it as is);
 *   - LLComboBox: an item must be SELECTED and its value string-equal (as
 *     LLComboBox::setValue early-outs); unselected text-entry content never counts;
 *   - everything else: LLSD equality (numbers by value, else string equivalence).
 * setTextIfChanged compares after the same CRLF removal LLTextBase::setText performs.
 * setLabelIfChanged(LLButton) compares both the selected and unselected labels.
 * * NO SETTING WRITES: the helpers only ever call the plain LLSD setValue / setText
 * overloads, which never call setControlValue (lluictrl.cpp) or commit. Do not add a
 * commit. (Never route through LLSlider::setValue(F32, from_event=false), which does
 * write the bound control_name setting.)
 *
 * RANGE ORDER: when a slider's range SHRINKS, call setMaxValueIfChanged BEFORE the
 * value write (the value is clamped to the old max otherwise); when it grows, after.
 *
 *   ALRevisionGate        "rebuild only when a watched U64 revision moved".
 *                         Honours the UIRefreshForceAlways debug setting and has a built-in
 *                         1 s self-heal (changed()/isStale() report true at least once a
 *                         second; lastWasFallback() says that is all it was), so every gate
 *                         in every panel follows both automatically.
 *   ALLiveReadoutClock    "true at most N times a second" for live numeric readouts.
 *   isWidgetOnScreen(w)   would LLView::drawChildren actually draw this widget?
 *
 * Typical use:
 *     // member:   ALRevisionGate mListGate;   ALLiveReadoutClock mReadoutClock{10.f};
 *     void MyPanel::draw()
 *     {
 *         LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
 *         if (mListGate.changed({ model.revision(), cast.revision() })) rebuildList();
 *         if (mReadoutClock.due()) {
 *             setMaxValueIfChanged(mSlider, model.maxValue());   // range before value
 *             setValueIfChanged(mSlider, model.value());
 *             setTextIfChanged(mLabel, model.statusText());
 *         }
 *         if (mBigText.stale && isWidgetOnScreen(mBigEditor)) refreshBigText();
 *         LLPanel::draw();
 *     }
 * and in onVisibilityChange(true): mListGate.reset() so the first frame after a
 * re-show is a full refresh instead of whatever the gate remembered.
 */

#ifndef AL_ALUIREFRESH_H
#define AL_ALUIREFRESH_H

#include "llview.h"
#include "lluictrl.h"
#include "llui.h"
#include "llfocusmgr.h"
#include "llf32uictrl.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llcombobox.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llrect.h"
#include "llsd.h"
#include "llsdutil.h"
#include "llstring.h"
#include "llframetimer.h"
#include "llviewercontrol.h"

#include <array>
#include <cmath>
#include <initializer_list>
#include <string>

// ---------------------------------------------------------------------------
// Debug switch
// ---------------------------------------------------------------------------
// UIRefreshForceAlways (Boolean, default 0, not persisted): every ALRevisionGate
// reports "changed" on every call, restoring per-frame refresh for stale-UI hunts.
inline bool alUiRefreshForceAlways()
{
    static LLCachedControl<bool> force_always(
        gSavedSettings, "UIRefreshForceAlways", false);
    return force_always;
}

// ---------------------------------------------------------------------------
// Focus handling
// ---------------------------------------------------------------------------
// True while the operator is typing into, or dragging, this control or any of its
// descendants.
inline bool alIsBeingEdited(const LLView* w)
{
    // an open combo dropdown is covered too: showList focuses the descendant list.
    return w && (gFocusMgr.childHasKeyboardFocus(w) ||
                 gFocusMgr.childHasMouseCapture(w));
}

// ---------------------------------------------------------------------------
// Comparison helpers
// ---------------------------------------------------------------------------
// Generic LLSD "same value": numbers by value, containers structurally, everything
// else by string equivalence (the way LLComboBox::setValue matches).
inline bool alSameValue(const LLSD& a, const LLSD& b)
{
    const bool a_num = a.isReal() || a.isInteger() || a.isBoolean();
    const bool b_num = b.isReal() || b.isInteger() || b.isBoolean();
    if (a_num && b_num)
    {
        return a.asReal() == b.asReal();
    }
    if (a.isMap() || a.isArray() || b.isMap() || b.isArray())
    {
        return llsd_equals(a, b);
    }
    return a.asString() == b.asString();
}

// The LLSlider behind a slider widget: the widget itself, or the LLSlider child of an
// LLSliderCtrl (child-list walk, no by-name lookup). nullptr for non-sliders.
inline const LLSlider* alInnerSlider(const LLUICtrl* w)
{
    if (const LLSlider* slider = dynamic_cast<const LLSlider*>(w))
    {
        return slider;
    }
    if (dynamic_cast<const LLSliderCtrl*>(w))
    {
        if (const LLView::child_list_t* kids = w->getChildList())
        {
            for (const LLView* kid : *kids)
            {
                if (const LLSlider* slider = dynamic_cast<const LLSlider*>(kid))
                {
                    return slider;
                }
            }
        }
    }
    return nullptr;
}

// EXACTLY what LLSlider::setValue(const LLSD&) stores for `v` (llslider.cpp):
//   wheel_fine_div > 0 : clamp, snap to min + n * (increment / div), pin the ends
//                        (snapToFineGrid -- no increment rounding);
//   otherwise          : clamp, then round to the nearest increment relative to min,
//                        biased down.
inline F32 alSliderStoredValue(const LLSlider* slider, F32 v)
{
    const F32 mn = slider->getMinValue();
    const F32 mx = slider->getMaxValue();
    const F32 inc = slider->getIncrement();
    const F32 div = slider->getWheelFineDiv();
    v = llclamp(v, mn, mx);
    if (div > 0.f)
    {
        if (inc > 0.f)
        {
            const F32 fine = inc / div;
            v = mn + ll_round(v - mn, fine);
            if (v >= mx - fine * 0.5f)
            {
                v = mx;
            }
            else if (v <= mn + fine * 0.5f)
            {
                v = mn;
            }
        }
        return v;
    }
    if (inc > 0.f)
    {
        v -= mn;
        v += inc / 2.0001f;
        v -= (F32)std::fmod(v, inc);
        v += mn;
    }
    return v;
}

// Would setValue(v) leave the widget showing what it already shows? Compares against
// what each widget's own setValue(const LLSD&) would store -- never clamps or rounds
// where the widget does not.
inline bool alValueMatches(const LLUICtrl* w, const LLSD& current, const LLSD& v)
{
    const bool cur_num = current.isReal() || current.isInteger();
    const bool new_num = v.isReal() || v.isInteger() || v.isBoolean();
    if (const LLSlider* slider = alInnerSlider(w))
    {
        if (cur_num && new_num)
        {
            // LLSlider::setValue stores "value" and compares getValueF32() != value
            // EXACTLY (llslider.cpp:128-133): no epsilon.
            const F32 cur = (F32)current.asReal();
            const F32 stored = alSliderStoredValue(slider, (F32)v.asReal());
            return cur == stored;
        }
    }
    else if (dynamic_cast<const LLF32UICtrl*>(w))
    {
        // LLSpinCtrl (and other LLF32UICtrl): setValue stores the RAW value.
        // An UNSET spinner (never set, or after clear()) writes even an equal value
        // (llspinctrl.cpp:297-305): hasBeenSet() false means always write.
        if (const LLSpinCtrl* spin = dynamic_cast<const LLSpinCtrl*>(w))
        {
            if (!spin->hasBeenSet())
            {
                return false;
            }
        }
        if (cur_num && new_num)
        {
            return (F32)current.asReal() == (F32)v.asReal();
        }
    }
    else if (const LLComboBox* combo = dynamic_cast<const LLComboBox*>(w))
    {
        // LLComboBox::setValue early-outs only when an ITEM is selected and its value
        // string-matches; getValue() may return unselected text-entry content, which
        // must not count as "already showing it".
        return combo->getCurrentIndex() >= 0 &&
               current.asString() == v.asString();
    }
    return alSameValue(current, v);
}
// ---------------------------------------------------------------------------
// Diff-guarded writes
// ---------------------------------------------------------------------------
template <class TextWidget>
inline bool setTextIfChanged(TextWidget* w, const std::string& s, bool force = false)
{
    if (!w)
    {
        return false;
    }
    // LLTextBase::setText strips CR/LF before storing; compare against the same form.
    std::string stripped;
    const std::string* wanted = &s;
    if (s.find('\r') != std::string::npos)
    {
        stripped = s;
        LLStringUtil::removeCRLF(stripped);
        wanted = &stripped;
    }
    if (w->getText() == *wanted)
    {
        return false;
    }
    if (!force && alIsBeingEdited(w))
    {
        return false;
    }
    w->setText(s);
    return true;
}

template <class Ctrl>
inline bool setValueIfChanged(Ctrl* w, const LLSD& v, bool force = false)
{
    if (!w || alValueMatches(w, w->getValue(), v))
    {
        return false;
    }
    if (!force && alIsBeingEdited(w))
    {
        return false;
    }
    w->setValue(v);     // the LLSD overload: never setControlValue, never commits
    return true;
}

template <class Widget>
inline bool setToolTipIfChanged(Widget* w, const std::string& tip)
{
    if (!w || w->getToolTip() == tip)
    {
        return false;
    }
    w->setToolTip(tip);
    return true;
}

template <class Widget>
inline bool setEnabledIfChanged(Widget* w, bool enabled)
{
    if (!w || w->getEnabled() == enabled)
    {
        return false;
    }
    w->setEnabled(enabled);
    return true;
}

template <class Widget>
inline bool setVisibleIfChanged(Widget* w, bool visible)
{
    if (!w || w->getVisible() == visible)
    {
        return false;
    }
    w->setVisible(visible);
    return true;
}

// Shrinking range: call this BEFORE setValueIfChanged (see header note).
template <class SliderLike>
inline bool setMaxValueIfChanged(SliderLike* w, F32 max_value)
{
    if (!w || w->getMaxValue() == max_value)
    {
        return false;
    }
    w->setMaxValue(max_value);
    return true;
}

inline bool setLabelIfChanged(LLButton* w, const std::string& label)
{
    // LLButton::setLabel replaces BOTH the selected and unselected labels: compare both
    if (!w || (w->getLabelUnselected() == label && w->getLabelSelected() == label))
    {
        return false;
    }
    w->setLabel(label);
    return true;
}

inline bool setLabelIfChanged(LLCheckBoxCtrl* w, const std::string& label)
{
    if (!w || w->getLabel() == label)
    {
        return false;
    }
    w->setLabel(label);
    return true;
}

// ---------------------------------------------------------------------------
// Revision gate
// ---------------------------------------------------------------------------
// Remembers up to kMax model revisions. changed() is true on the very first call,
// after reset(), and whenever any watched revision differs from the one last
// accepted; it records the new values when it returns true.
//
// SELF-HEAL (built in): changed()/isStale() ALSO return true at least once per
// kFallbackSeconds even when no revision moved, so a model mutator that forgot to
// bump its revision can never leave a panel stale for more than a second. Callers
// that can cheaply verify the real state (a signature, a value diff) should do so
// when lastWasFallback() is true and skip the rebuild if nothing differs; callers
// whose rebuild is cheap can ignore it. lastWasFallback() is also true for the
// UIRefreshForceAlways debug setting (every call reports changed, "unverified").
//
// Deferred work (e.g. "rebuild when scrolled into view"): use isStale() to ask and
// accept() once the rebuild has actually happened, so the pending state survives the
// frames in which the work was not allowed to run. accept() restarts the fallback timer.
struct ALRevisionGate
{
    static constexpr size_t kMax = 6;
    static constexpr F32 kFallbackSeconds = 1.f;

    // Check + record. `force` is a per-call extra switch (a panel-specific debug
    // setting); it is treated like the global UIRefreshForceAlways.
    bool changed(std::initializer_list<U64> now, bool force = false)
    {
        bool stale = isStale(now);
        if (!stale && force)
        {
            stale = true;
            mFallbackOnly = true;
        }
        if (stale)
        {
            accept(now);
        }
        return stale;
    }

    bool isStale(std::initializer_list<U64> now)
    {
        mFallbackOnly = false;
        if (!mPrimed)
        {
            return true;
        }
        size_t i = 0;
        for (U64 value : now)
        {
            if (i < kMax && mSeen[i] != value)
            {
                return true;
            }
            ++i;
        }
        if (alUiRefreshForceAlways() ||
            mFallbackTimer.getElapsedTimeF32() >= kFallbackSeconds)
        {
            mFallbackOnly = true;
            return true;
        }
        return false;
    }

    // True when the last changed()/isStale() reported "stale" ONLY because the
    // fallback timer / force switch fired (no watched revision moved).
    bool lastWasFallback() const { return mFallbackOnly; }

    void accept(std::initializer_list<U64> now)
    {
        size_t i = 0;
        for (U64 value : now)
        {
            if (i < kMax)
            {
                mSeen[i] = value;
            }
            ++i;
        }
        mPrimed = true;
        mFallbackTimer.reset();
    }

    // Next changed()/isStale() reports a change (call from onVisibilityChange(true)).
    void reset() { mPrimed = false; }

private:
    std::array<U64, kMax> mSeen{};
    LLFrameTimer mFallbackTimer;
    bool mPrimed = false;
    bool mFallbackOnly = false;
};
// ---------------------------------------------------------------------------
// Live readout clock
// ---------------------------------------------------------------------------
// due() is true at most `hz` times a second (first call is always due). Use it to
// throttle numeric readouts that mirror fast-changing model state; pair every write
// with the *IfChanged helpers so an unchanged value never touches the widget.
struct ALLiveReadoutClock
{
    explicit ALLiveReadoutClock(F32 hz = 10.f) { setRate(hz); }

    void setRate(F32 hz) { mPeriod = hz > 0.f ? 1.f / hz : 0.f; }

    bool due()
    {
        if (mFirst || mTimer.getElapsedTimeF32() >= mPeriod)
        {
            mFirst = false;
            mTimer.reset();
            return true;
        }
        return false;
    }

    // Make the next due() true (selection change, panel re-show).
    void forceDue() { mFirst = true; }

private:
    LLFrameTimer mTimer;
    F32 mPeriod = 0.1f;
    bool mFirst = true;
};

// ---------------------------------------------------------------------------
// On-screen test
// ---------------------------------------------------------------------------
// Mirrors the test LLView::drawChildren() applies to every child on the way down
// from the root view: the view is visible, its rect is valid, and its screen rect
// overlaps both the root's local rect and LLView::sDirtyRect. The test is applied
// to the widget and to every ancestor; a chain that never reaches the root view
// (detached widget) is not on screen.
//
// Beyond drawChildren (clip_to_ancestors, default true): the widget's screen rect
// must also overlap each ancestor's screen rect. drawChildren does not test this
// because LLScrollContainer clips with a scissor instead of skipping the draw, so a
// widget scrolled out of its container's viewport still "draws" (into nothing).
// For refresh decisions that is exactly the case we want to treat as off screen.
inline bool isWidgetOnScreen(const LLView* view, bool clip_to_ancestors = true)
{
    if (!view)
    {
        return false;
    }
    LLView* root = LLUI::getInstance()->getRootView();
    if (!root)
    {
        return false;
    }
    const LLRect root_rect = root->getLocalRect();
    const LLRect view_screen = view->calcScreenRect();

    bool reached_root = false;
    for (const LLView* cur = view; cur; cur = cur->getParent())
    {
        if (cur == root)
        {
            reached_root = true;
            break;
        }
        if (!cur->getVisible() || !cur->getRect().isValid())
        {
            return false;
        }
        const LLRect screen = (cur == view) ? view_screen : cur->calcScreenRect();
        if (!root_rect.overlaps(screen) || !LLView::sDirtyRect.overlaps(screen))
        {
            return false;
        }
        if (clip_to_ancestors && cur != view && !screen.overlaps(view_screen))
        {
            return false;
        }
    }
    return reached_root;
}

#endif // AL_ALUIREFRESH_H
