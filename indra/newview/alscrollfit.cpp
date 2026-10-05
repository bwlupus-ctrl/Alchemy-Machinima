/**
 * @file alscrollfit.cpp
 * @brief Shared per-tab scroll machinery (wrap, auto-fit, focus-reveal, wheel rule).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [ScrollFit] Extracted from the Lightbox's [LightboxWheel] block; behaviour unchanged.
 * See alscrollfit.h.
 */

#include "llviewerprecompiledheaders.h"
#include "alscrollfit.h"

#include "alscrollfocus.h"

#include <algorithm>

#include "lltimer.h"
#include "llpanel.h"
#include "llscrollbar.h"
#include "llsliderctrl.h"
#include "llslider.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"
#include "llviewborder.h"
#include "llviewercontrol.h"

namespace ALScrollFit
{
namespace
{
// The slider (or slider ctrl's inner slider) and the nearest scroll container under
// the cursor, in the floater's local coordinates.
void findWheelTargets(LLView* floater, S32 x, S32 y, LLSlider*& slider, LLScrollContainer*& container)
{
    slider = nullptr;
    container = nullptr;
    for (LLView* v = floater->childFromPoint(x, y, true); v && v != floater; v = v->getParent())
    {
        if (!slider)
        {
            slider = dynamic_cast<LLSlider*>(v);
            if (!slider && dynamic_cast<LLSliderCtrl*>(v))
            {
                // wheel-forwarding slider ctrls (wheel_fine_div) route via their inner slider
                for (LLView* c : *v->getChildList())
                {
                    if ((slider = dynamic_cast<LLSlider*>(c)) != nullptr) break;
                }
            }
        }
        container = dynamic_cast<LLScrollContainer*>(v);
        if (container) break;
    }
}
} // namespace

LLView* scrolledView(LLScrollContainer* sc)
{
    const LLRect doc_rect = sc->getScrolledViewRect();
    for (LLView* child : *sc->getChildList())
    {
        if (child && child->getRect() == doc_rect
            && !dynamic_cast<LLScrollbar*>(child) && !dynamic_cast<LLViewBorder*>(child))
        {
            return child;
        }
    }
    return nullptr;
}

bool hasDirectScrollContainer(LLView* tab)
{
    for (LLView* child : *tab->getChildList())
    {
        if (dynamic_cast<LLScrollContainer*>(child))
        {
            return true;
        }
    }
    return false;
}

void collectTabContainers(LLTabContainer* tabs, std::vector<LLScrollContainer*>& out)
{
    for (S32 i = 0, count = tabs->getTabCount(); i < count; ++i)
    {
        LLPanel* tab = tabs->getPanelByIndex(i);
        if (!tab) continue;
        for (LLView* child : *tab->getChildList())
        {
            if (LLScrollContainer* sc = dynamic_cast<LLScrollContainer*>(child))
            {
                out.push_back(sc);
            }
        }
    }
}

void collectDocumentContainers(LLView* root, std::vector<LLScrollContainer*>& out)
{
    for (LLView* child : *root->getChildList())
    {
        if (!child) continue;
        if (LLScrollContainer* sc = dynamic_cast<LLScrollContainer*>(child))
        {
            if (dynamic_cast<LLPanel*>(sc->getParent()) && dynamic_cast<LLPanel*>(scrolledView(sc))
                && std::find(out.begin(), out.end(), sc) == out.end())
            {
                out.push_back(sc);
            }
        }
        collectDocumentContainers(child, out);
    }
}

void fitContainer(LLScrollContainer* sc, const StretcherMins* mins)
{
    LLView* doc = scrolledView(sc);
    if (!doc) return;

    const LLRect doc_rect = doc->getRect();
    const S32 old_h = doc_rect.getHeight();
    bool any = false;
    S32 need = 1;
    for (LLView* child : *doc->getChildList())
    {
        if (!child) continue;
        any = true; // visibility ignored on purpose: rows toggled at runtime keep their slot
        const LLRect& r = child->getRect();
        if (child->followsTop() && !child->followsBottom())
        {
            // moves with the top edge: its bottom ends at (r.mBottom + new_h - old_h)
            need = llmax(need, old_h - r.mBottom + FIT_PAD);
        }
        else if (!child->followsTop())
        {
            // anchored to the bottom (or nothing): stays put, must still fit inside
            need = llmax(need, r.mTop + FIT_PAD);
        }
        else
        {
            // follows top AND bottom: stretches with the document (height after =
            // r.height + new_h - old_h); keep it >= its wrap-time natural height (1 px when
            // none was recorded) so a follows="all" panel is never collapsed by a shrink.
            S32 min_h = 1;
            if (mins)
            {
                const auto m = mins->find(child);
                if (m != mins->end()) min_h = llmax(1, m->second);
            }
            need = llmax(need, old_h - r.getHeight() + min_h + FIT_PAD);
        }
    }
    if (any && need != old_h)
    {
        doc->reshape(doc_rect.getWidth(), need, true);
        doc->translate(0, doc_rect.mTop - doc->getRect().mTop); // keep the top edge
    }
    // Re-derive doc size / page size / scrollbar visibility.
    sc->reshape(sc->getRect().getWidth(), sc->getRect().getHeight(), true);
}

// [DirFix] H2: see alscrollfit.h. Same per-child rules as fitContainer, applied to a panel.
S32 fitPanelToChildren(LLPanel* panel, S32 pad)
{
    if (!panel) return 0;

    pad = llmax(0, pad);
    const LLRect panel_rect = panel->getRect();
    const S32 old_h = panel_rect.getHeight();
    bool any = false;
    S32 need = 1;
    for (LLView* child : *panel->getChildList())
    {
        if (!child) continue;
        any = true; // visibility ignored on purpose
        const LLRect& r = child->getRect();
        if (child->followsTop() && !child->followsBottom())
        {
            need = llmax(need, old_h - r.mBottom + pad);
        }
        else if (!child->followsTop())
        {
            need = llmax(need, r.mTop + pad);
        }
        else
        {
            // follows top AND bottom: stretches with the panel; counted at its 1-px floor.
            need = llmax(need, old_h - r.getHeight() + 1 + pad);
        }
    }
    if (any && need != old_h)
    {
        panel->reshape(panel_rect.getWidth(), need, true);
        panel->translate(0, panel_rect.mTop - panel->getRect().mTop); // keep the top edge
    }
    return panel->getRect().getHeight();
}

LLScrollContainer* wrapTabPanel(LLPanel* tab, StretcherMins* mins)
{
    if (!tab || hasDirectScrollContainer(tab)) return nullptr;

    const LLRect local = tab->getLocalRect();

    LLScrollContainer::Params sp;
    sp.name = tab->getName() + "_auto_scroll";
    sp.rect = local;
    sp.follows.flags(FOLLOWS_ALL);
    sp.is_opaque = false;
    sp.reserve_scroll_corner = false;
    LLScrollContainer* sc = LLUICtrlFactory::create<LLScrollContainer>(sp);
    if (!sc) return nullptr;

    LLPanel::Params pp;
    pp.name = tab->getName() + "_auto_scroll_content";
    pp.rect = local;
    pp.follows.flags(FOLLOWS_LEFT | FOLLOWS_TOP | FOLLOWS_RIGHT);
    pp.background_visible = false;
    LLPanel* doc = LLUICtrlFactory::create<LLPanel>(pp);
    if (!doc)
    {
        delete sc;
        return nullptr;
    }
    sc->addChild(doc); // first child becomes the scrolled view

    // Snapshot (frontmost first), then re-add back-to-front to keep the child order.
    std::vector<std::pair<LLView*, S32>> kids;
    for (LLView* child : *tab->getChildList())
    {
        if (!child) continue;
        const LLView::child_tab_order_t& order = tab->getTabOrder();
        const LLView::child_tab_order_const_iter_t it = order.find(child);
        kids.emplace_back(child, it != order.end() ? it->second : 0);
        // Natural height of a stretcher, taken before any reparent/reshape touches it.
        if (mins && child->followsTop() && child->followsBottom())
        {
            (*mins)[child] = child->getRect().getHeight();
        }
    }
    for (auto it = kids.rbegin(); it != kids.rend(); ++it)
    {
        doc->addChild(it->first, it->second); // removes it from the tab panel
    }

    tab->addChild(sc);

    const S32 sb = llmax(0, gSavedSettings.getS32("UIScrollbarSize"));
    doc->reshape(llmax(1, local.getWidth() - sb), local.getHeight(), true);

    // Focus-reveal is installed separately (FocusTracker::installTabLevel).
    fitContainer(sc, mins);
    return sc;
}

void collectTabContainerTree(LLTabContainer* tabs, std::vector<LLTabContainer*>& out)
{
    if (!tabs) return;
    out.push_back(tabs);
    // Walk every descendant; a nested tab container is recorded and walked in turn.
    std::function<void(LLView*)> walk = [&](LLView* v)
    {
        for (LLView* child : *v->getChildList())
        {
            if (!child) continue;
            if (LLTabContainer* tc = dynamic_cast<LLTabContainer*>(child))
            {
                out.push_back(tc);
            }
            walk(child);
        }
    };
    walk(tabs);
}

void fitAll(LLView* root, LLTabContainer* tabs, const StretcherMins* mins)
{
    std::vector<LLScrollContainer*> containers;
    // Every tab-level scroller (any document type, nested tab containers included) first,
    // then the other panel-in-panel document containers; internal widget scrollers are
    // skipped.
    std::vector<LLTabContainer*> all_tabs;
    collectTabContainerTree(tabs, all_tabs);
    for (LLTabContainer* tc : all_tabs)
    {
        collectTabContainers(tc, containers);
    }
    if (root)
    {
        collectDocumentContainers(root, containers);
    }
    for (LLScrollContainer* sc : containers)
    {
        fitContainer(sc, mins);
    }
}

// ---- FocusTracker -------------------------------------------------------------------

bool FocusTracker::installed(const LLScrollContainer* sc) const
{
    return std::find(mDone.begin(), mDone.end(), sc) != mDone.end();
}

bool FocusTracker::install(LLScrollContainer* sc, LLView* document, LLView* parent)
{
    if (!sc || installed(sc)) return false;
    ALScrollFocus::install(sc, document, parent);
    mDone.push_back(sc);
    return true;
}

bool FocusTracker::installNamed(LLView* owner, const char* scroller_name, const char* document_name)
{
    if (!owner) return false;
    if (LLScrollContainer* sc = owner->findChild<LLScrollContainer>(scroller_name))
    {
        if (installed(sc)) return false;
        mDone.push_back(sc);
    }
    // Unchanged from the Lightbox: a missing name still goes through the getChild-based
    // overload (which logs), exactly as before.
    ALScrollFocus::install(owner, scroller_name, document_name);
    return true;
}

void FocusTracker::installTabLevel(LLTabContainer* tabs)
{
    if (!tabs) return;
    std::vector<LLTabContainer*> all_tabs;
    collectTabContainerTree(tabs, all_tabs);
    std::vector<LLScrollContainer*> tab_level;
    for (LLTabContainer* tc : all_tabs)
    {
        collectTabContainers(tc, tab_level);
    }
    for (LLScrollContainer* sc : tab_level)
    {
        if (installed(sc)) continue;
        if (LLView* doc = scrolledView(sc))
        {
            ALScrollFocus::install(sc, doc, doc);
            mDone.push_back(sc);
        }
    }
}

// ---- wheel --------------------------------------------------------------------------

void enableSliderWheel(LLView* root)
{
    for (LLView* child : *root->getChildList())
    {
        if (!child) continue;
        if (LLSlider* slider = dynamic_cast<LLSlider*>(child))
        {
            slider->setWheelAdjust(true);
        }
        enableSliderWheel(child);
    }
}

bool WheelGuard::guardActive(const LLSlider* slider, const LLScrollContainer* container) const
{
    return slider && container && container == mContainer
        && (LLTimer::getTotalSeconds() - mTime) < WHEEL_GUARD_SEC;
}

void WheelGuard::arm(const LLScrollContainer* container)
{
    mContainer = container;
    mTime = LLTimer::getTotalSeconds();
}

void WheelGuard::reset()
{
    mContainer = nullptr;
    mTime = -1.0e9;
}

bool WheelGuard::onScrollWheel(LLView* root, S32 x, S32 y, const std::function<bool()>& base)
{
    LLSlider* slider = nullptr;
    LLScrollContainer* container = nullptr;
    findWheelTargets(root, x, y, slider, container);

    const bool guard = guardActive(slider, container);
    const bool saved_adjust = slider ? slider->getWheelAdjust() : false;
    if (guard)
    {
        slider->setWheelAdjust(false);
    }
    const S32 v_before = container ? container->getDocPosVertical() : 0;
    const S32 h_before = container ? container->getDocPosHorizontal() : 0;
    const bool handled = base();
    if (guard)
    {
        slider->setWheelAdjust(saved_adjust);
    }
    // Arm on a real scroll; a guard-redirected event also keeps it alive, so spinning on at
    // the end of the scroll range over a slider never starts adjusting it.
    if (container && (guard
        || container->getDocPosVertical() != v_before || container->getDocPosHorizontal() != h_before))
    {
        arm(container);
    }
    return handled;
}

bool WheelGuard::onScrollHWheel(LLView* root, S32 x, S32 y, const std::function<bool()>& base)
{
    LLSlider* slider = nullptr;
    LLScrollContainer* container = nullptr;
    findWheelTargets(root, x, y, slider, container);
    if (guardActive(slider, container))
    {
        mTime = LLTimer::getTotalSeconds(); // a swallowed event keeps the guard alive
        return true;
    }
    const S32 v_before = container ? container->getDocPosVertical() : 0;
    const S32 h_before = container ? container->getDocPosHorizontal() : 0;
    const bool handled = base();
    if (container
        && (container->getDocPosVertical() != v_before || container->getDocPosHorizontal() != h_before))
    {
        arm(container);
    }
    return handled;
}

// ---- Controller ---------------------------------------------------------------------

void Controller::installForTabs(LLTabContainer* tabs, LLView* root, const std::vector<NamedScroller>& named)
{
    // Every tab panel without a scroll container (and any tab added later) gets one
    // wrapped around its children before anything else looks at the hierarchy. Nested tab
    // containers are discovered first (the walk is pointer-stable across the reparenting
    // wrapTabPanel does) and handled the same way, each exactly once.
    std::vector<LLTabContainer*> all_tabs;
    collectTabContainerTree(tabs, all_tabs);
    for (LLTabContainer* tc : all_tabs)
    {
        if (std::find(mTabs.begin(), mTabs.end(), tc) != mTabs.end()) continue; // one-shot
        mTabs.push_back(tc);
        for (S32 i = 0, count = tc->getTabCount(); i < count; ++i)
        {
            wrapTabPanel(tc->getPanelByIndex(i), mMins.get());
        }
        // Re-fit when the active tab changes (commit fires on tab select). Appended, so
        // any XML-defined commit callback of this container is kept. The callback holds its
        // own reference to the stretcher minimums.
        std::shared_ptr<StretcherMins> mins = mMins;
        tc->setCommitCallback([root, tabs, mins](LLUICtrl*, const LLSD&) { fitAll(root, tabs, mins.get()); });
    }
    // Explicit (XML-named) focus installs first, then every other tab-level container once.
    for (const NamedScroller& n : named)
    {
        mFocus.installNamed(root, n.scroller, n.document);
    }
    mFocus.installTabLevel(tabs);

    // Fit every scroll document to its children (hand-typed XML heights are ignored) and
    // enable wheel adjust on every slider under root.
    fitAll(root, tabs, mMins.get());
    if (root)
    {
        enableSliderWheel(root);
    }
}
} // namespace ALScrollFit
