/**
 * @file alscrollfit.h
 * @brief Shared per-tab scroll machinery (wrap, auto-fit, focus-reveal, wheel rule).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [ScrollFit] Promoted verbatim from the Lightbox's [LightboxWheel] namespace block
 * (alfloaterlightbox.cpp, commit 9f31a1f2058) so any tabbed floater can use it.
 * Rationale: doc/DIRECTOR_CONSOLE_AUDIT_FABLE.md, fix-plan step H1.
 *
 * All of this runs at build / open / tab-change / input-event time. None of it runs in
 * draw() (CLAUDE.md: UI PANELS MUST NOT DO WORK IN draw()).
 *
 * ONE-CALL USE (a floater owns an ALScrollFit::Controller):
 *     // member:  ALScrollFit::Controller mScrollFit;
 *     // postBuild(), once the XML tree exists:
 *     mScrollFit.installForTabs(findChild<LLTabContainer>("tabs"), this);
 *     // onOpen(): mScrollFit.fit(this, tabs);                 (cheap, idempotent)
 *     // [SliderWheel] no wheel overrides: the scroll-through guard is global in llui.
 *
 * installForTabs does, in this order: wrap every tab without a DIRECT-child scroller
 * (wrapTabPanel), re-fit on tab change, install focus-reveal on the named scrollers and
 * then once on every tab-level container (FocusTracker), fit every document (fitAll) and
 * enable wheel adjust on every LLSlider under `root` (enableSliderWheel).
 *
 * LIFETIME: the tab-change commit callback captures `tabs` and `root`; `root` must be an
 * ancestor of (or equal to) `tabs`, so both die together.
 */

#ifndef AL_SCROLLFIT_H
#define AL_SCROLLFIT_H

#include "llscrollcontainer.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class LLPanel;
class LLSlider;
class LLTabContainer;

namespace ALScrollFit
{
// Gap kept below the lowest child of a scroll document.
constexpr S32 FIT_PAD = 12;
// ---- (a)/(b)/(c) layout -------------------------------------------------------------

// The document (scrolled view) of a scroll container, or nullptr. mScrolledView is
// protected, so this matches the child whose rect is getScrolledViewRect() (scrollbars and
// border excluded).
LLView* scrolledView(LLScrollContainer* sc);

// True when `tab` has a DIRECT-child LLScrollContainer. Nested/internal scrollers (text
// editors, lists) deliberately do not count.
bool hasDirectScrollContainer(LLView* tab);

// Wrap-time minimum heights for "stretcher" children (follows top AND bottom). Recorded by
// wrapTabPanel from each stretcher's original rect height and honoured by fitContainer, so
// a stretcher never shrinks below its natural height. Owned by the Controller (keyed by
// child pointer; a child is only ever recorded while it lives in its floater).
using StretcherMins = std::unordered_map<const LLView*, S32>;

// (a) Gives a panel that has no direct-child scroll container one: a follows-all scroll
// container holding a document panel that adopts the panel's children (rects, order and
// tab order preserved). The document is narrowed by the scrollbar width so right-following
// controls land left of the scrollbar, then fitted. No-op (returns nullptr) when the panel
// already has a direct scroller. Returns the new container, or nullptr. When `mins` is
// given, the natural (pre-wrap) height of every follows-top-AND-bottom child is recorded in
// it BEFORE the panel is reparented/reshaped (see fitContainer).
LLScrollContainer* wrapTabPanel(LLPanel* tab, StretcherMins* mins = nullptr);

// (b) Resizes the document so its height = lowest child extent + FIT_PAD (grows AND
// shrinks, never below 1 px), keeping the document's top edge in place, then makes the
// container recompute its scroll range / scrollbar visibility. Follows-flag rules: a
// child that follows top only moves with the top edge, a child that follows neither top
// nor bottom stays put, a child that follows top AND bottom stretches with the document and
// keeps >= its recorded minimum height in `mins` (its natural height at wrap time; 1 px when
// not recorded): the document is at least (top gap + bottom gap + natural height + FIT_PAD)
// tall, so a follows="all" panel that filled a tab is never collapsed when the document is
// later shrunk. Idempotent: refitting a fitted document changes nothing.
// Child visibility is ignored on purpose (runtime-toggled rows keep their slot).
void fitContainer(LLScrollContainer* sc, const StretcherMins* mins = nullptr);

// [DirFix] H2: sizes a PANEL (not a scroll document) to its own direct children, e.g. a flow
// grid card (title band + embedded shared panel). Same follows rules as fitContainer, with
// `pad` in place of FIT_PAD and no recorded minimums (a follows-top-AND-bottom child counts
// as 1 px): need = max over direct children of
//   follows top only      -> (old_h - child.mBottom) + pad   (moves with the top edge)
//   follows neither/bottom -> child.mTop + pad               (stays put)
//   follows top AND bottom -> (old_h - child.height) + 1 + pad.
// Child visibility is ignored. Grows AND shrinks (never below 1 px), keeps the panel's top
// edge in place, and is idempotent. A panel with no children is left unchanged. Returns the
// panel's height after the call (0 for a null panel).
S32 fitPanelToChildren(LLPanel* panel, S32 pad);

// (c) Tab-level containers: the direct-child scrollers of each tab panel (any document
// type). Appends to `out` (no de-duplication; a scroller is only ever one tab's child).
void collectTabContainers(LLTabContainer* tabs, std::vector<LLScrollContainer*>& out);

// (c) Other "document"-style containers anywhere under `root` (panel inside a panel, e.g.
// Actor Mask nested in a tab); skips the internal scrollers of text widgets/lists (only a
// scroller whose parent AND scrolled view are LLPanels counts). Appends without duplicates.
void collectDocumentContainers(LLView* root, std::vector<LLScrollContainer*>& out);

// `tabs` itself followed by every LLTabContainer nested anywhere below it (any depth),
// in document order. Appends to `out`. `tabs` may be null (nothing appended).
void collectTabContainerTree(LLTabContainer* tabs, std::vector<LLTabContainer*>& out);

// Fits every tab-level container of `tabs` and of any tab container nested below it (may
// be null), then every other document container under `root`. Cheap and idempotent; safe
// at postBuild/onOpen/tab change.
void fitAll(LLView* root, LLTabContainer* tabs, const StretcherMins* mins = nullptr);

// Refit only the newly visible tab page. This is the tab-commit fast path;
// install/open still use fitAll so initially hidden pages retain identical sizing.
void fitPage(LLView* page, const StretcherMins* mins = nullptr);

// Sizes a direct child to fill the usable area of an unwrapped tab page. The
// child's left inset is preserved; its top edge is placed at `top_px` from the
// page top (LLView origin y == 0 for that placement in this coordinate system).
void fillPage(LLView* page, LLView* child, S32 top_px);

// ---- (d) focus-reveal ---------------------------------------------------------------

// Container-level bookkeeping on top of ALScrollFocus::install, which is itself idempotent
// per (control, scroller) (alscrollfocus.h detail::claim: a weak token owned by the focus
// slot, so a control already covered -- e.g. by installAncestor from an embedded panel --
// gets no second callback). The tracker additionally skips whole containers it already
// covered. Owned by the floater (Controller holds one).
class FocusTracker
{
public:
    bool installed(const LLScrollContainer* sc) const;

    // Install on `sc` (document/parent as ALScrollFocus::install) unless already covered.
    // Returns true when it installed.
    bool install(LLScrollContainer* sc, LLView* document, LLView* parent);

    // Name-based install (as ALScrollFocus::install(owner, names)); records the container
    // it covered. A container already covered is skipped (returns false).
    bool installNamed(LLView* owner, const char* scroller_name, const char* document_name);

    // Installs on every tab-level container of `tabs` and of nested tab containers (any
    // depth) not yet covered, using the container's own document as document and parent.
    void installTabLevel(LLTabContainer* tabs);

    void clear() { mDone.clear(); }

private:
    std::vector<LLScrollContainer*> mDone; // identity only
};

// ---- (e) wheel rule -----------------------------------------------------------------

// Enables wheel adjust on every LLSlider under `root` (recursive) = explicit ON (legacy step,
// ignores the UISliderWheelAdjust switch).
// [SliderWheel] The per-floater WheelGuard that used to live here is RETIRED: the scroll-through
// guard is global in llui now (indra/llui/llsliderwheel.h, doc/SLIDER_WHEEL_GLOBAL_DESIGN.md), so
// a floater needs no handleScrollWheel / handleScrollHWheel overrides for it.
void enableSliderWheel(LLView* root);

// ---- (f) one-call convenience -------------------------------------------------------

struct NamedScroller
{
    const char* scroller;  // LLScrollContainer name
    const char* document;  // its document view name
};

// What a floater owns. See the file header for the call sequence.
class Controller
{
public:
    // Tab pages installForTabs must not wrap (by LLPanel name): group pages holding a
    // follows="all" nested tab container, and pages hosting a self-scrolling shared
    // panel. Call before installForTabs.
    void setUnwrappedPages(std::vector<std::string> names) { mUnwrapped = std::move(names); }

    // (f) wrap + re-fit-on-tab-change + focus + fit + wheel for every tab of `tabs` AND of
    // every tab container nested below it (each nested container's panels are wrapped and
    // fitted too). One-shot per tab container: wrapping and the tab-change commit callback
    // are installed once per container however often this is called (further calls only
    // re-run the idempotent focus/fit/wheel steps). The callbacks are APPENDED (existing XML
    // commit callbacks are kept) and refit only the newly visible page (including the
    // current page of any nested tab container beneath it).
    // (`tabs` may be null: then only `named` focus installs, fitAll over `root's document
    // containers and slider wheel enabling run). `named` containers get focus-reveal
    // first (with their XML-named document), then every other tab-level container once.
    void installForTabs(LLTabContainer* tabs, LLView* root,
                        const std::vector<NamedScroller>& named = {});

    // Re-fit with this controller's recorded stretcher minimums (use from onOpen etc.
    // instead of the bare fitAll, which knows no minimums).
    void fit(LLView* root, LLTabContainer* tabs) { fitAll(root, tabs, mMins.get()); }
    void fitPage(LLView* page) { ALScrollFit::fitPage(page, mMins.get()); }
    void setCommitFitEnabled(bool enabled) { *mCommitFitEnabled = enabled; }

    void reset() { mFocus.clear(); mMins->clear(); }

    FocusTracker& focus() { return mFocus; }

private:
    FocusTracker mFocus;
    // Shared with the tab-change callbacks (they may outlive this object during floater
    // teardown, so they hold their own reference).
    std::shared_ptr<StretcherMins> mMins = std::make_shared<StretcherMins>();
    std::shared_ptr<bool> mCommitFitEnabled = std::make_shared<bool>(true);
    std::vector<LLTabContainer*> mTabs; // containers already wrapped + callback-installed
    std::vector<std::string> mUnwrapped;
};
} // namespace ALScrollFit

#endif // AL_SCROLLFIT_H
