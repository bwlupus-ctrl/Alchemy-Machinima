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
#include "alkitpage.h"

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
// [SliderWheel] findWheelTargets moved to LLSliderWheel::sliderAt (indra/llui/llsliderwheel.cpp).

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
    if (dynamic_cast<SelfSizedDocument*>(doc)) return;

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
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALScrollFit fitAll");
    std::vector<LLScrollContainer*> containers;
    // Every tab-level scroller (any document type, nested tab containers included) first,
    // then the other panel-in-panel document containers; internal widget scrollers are
    // skipped.
    std::vector<LLTabContainer*> all_tabs;
    collectTabContainerTree(tabs, all_tabs);
    for (LLTabContainer* tc : all_tabs)
    {
        for (S32 i = 0, count = tc->getTabCount(); i < count; ++i)
        {
            LLPanel* page = tc->getPanelByIndex(i);
            if (!page) continue;
            for (LLView* child : *page->getChildList())
            {
                if (LLScrollContainer* sc = dynamic_cast<LLScrollContainer*>(child))
                {
                    // A named XML scroller may have been constructed before its tab
                    // received its final saved-floater geometry.  Follows flags only
                    // propagate later deltas, so establish the tab-local invariant now.
                    sc->setShape(page->getLocalRect());
                }
            }
        }
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

void fitPage(LLView* page, const StretcherMins* mins)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALScrollFit fitPage");
    if (!page)
    {
        return;
    }
    std::vector<LLScrollContainer*> containers;
    std::function<void(LLView*, bool)> collect_visible_page =
        [&](LLView* root, bool direct_tab_page)
    {
        if (!root) return;
        if (LLTabContainer* nested_tabs = dynamic_cast<LLTabContainer*>(root))
        {
            collect_visible_page(nested_tabs->getCurrentPanel(), true);
            return;
        }
        for (LLView* child : *root->getChildList())
        {
            if (!child) continue;
            if (LLScrollContainer* sc = dynamic_cast<LLScrollContainer*>(child))
            {
                if (direct_tab_page)
                {
                    // Keep named and auto-wrapped scrollers under the same rule
                    // for the main page and every visible nested tab page.
                    sc->setShape(root->getLocalRect());
                }
                LLView* document = scrolledView(sc);
                // A direct tab-page scroller accepts any document type, just
                // like collectTabContainers(). Deeper document scrollers keep
                // collectDocumentContainers()'s panel-in-panel filter.
                if ((direct_tab_page ||
                     (dynamic_cast<LLPanel*>(sc->getParent()) &&
                      dynamic_cast<LLPanel*>(document))) &&
                    std::find(containers.begin(), containers.end(), sc) == containers.end())
                {
                    containers.push_back(sc);
                }
                // Scrolled documents can host another tab container (Ghost
                // Studio) and further document scrollers. Follow that visible
                // chain so this fast path has the old fitAll geometry.
                collect_visible_page(document, false);
                continue;
            }
            collect_visible_page(child, false);
        }
    };
    collect_visible_page(page, true);
    for (LLScrollContainer* sc : containers)
    {
        fitContainer(sc, mins);
    }
}

void fillPage(LLView* page, LLView* child, S32 top_px)
{
    if (!page || !child) return;
    const S32 w = page->getLocalRect().getWidth() - child->getRect().mLeft;
    const S32 h = page->getLocalRect().getHeight() - top_px;
    child->reshape(llmax(1, w), llmax(1, h), true);
    child->setOrigin(child->getRect().mLeft, 0);
}

// ---- FocusTracker -------------------------------------------------------------------

bool FocusTracker::installed(const LLScrollContainer* sc) const
{
    return std::find(mDone.begin(), mDone.end(), sc) != mDone.end();
}

bool FocusTracker::install(LLScrollContainer* sc, LLView* document, LLView* parent)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALScrollFocus install");
    if (!sc || installed(sc)) return false;
    // ALKitScroller owns a pointer-driven v2 page. Its button captures before
    // taking focus, so synchronous focus reveal would move the pressed control.
    // Legacy/stock scrollers retain their existing focus-reveal behaviour.
    ALScrollFocus::install(sc, document, parent, !dynamic_cast<ALKitScroller*>(sc));
    mDone.push_back(sc);
    return true;
}

bool FocusTracker::installNamed(LLView* owner, const char* scroller_name, const char* document_name)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALScrollFocus install");
    if (!owner) return false;
    if (LLScrollContainer* sc = owner->findChild<LLScrollContainer>(scroller_name))
    {
        if (installed(sc)) return false;
        LLView* document = owner->getChildView(document_name);
        return install(sc, document, document);
    }
    // Unchanged from the Lightbox: a missing name still goes through the getChild-based
    // overload (which logs), exactly as before.
    ALScrollFocus::install(owner, scroller_name, document_name);
    return true;
}

void FocusTracker::installTabLevel(LLTabContainer* tabs)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALScrollFocus install");
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
            install(sc, doc, doc);
        }
    }
}

// ---- wheel --------------------------------------------------------------------------

// [SliderWheel] WheelGuard is RETIRED: the scroll-through guard is now global (llui,
// indra/llui/llsliderwheel.h: LLScrollbar::scrollByWheel stamps it, sliders consult it, and
// LLScrollContainer keeps it alive over disabled sliders). enableSliderWheel stays: explicit ON =
// the legacy step, and it ignores the UISliderWheelAdjust kill switch.
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

// ---- Controller ---------------------------------------------------------------------

void Controller::installForTabs(LLTabContainer* tabs, LLView* root, const std::vector<NamedScroller>& named)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALScrollFit installForTabs");
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
            LLPanel* page = tc->getPanelByIndex(i);
            if (page && std::find(mUnwrapped.begin(), mUnwrapped.end(), page->getName()) == mUnwrapped.end())
            {
                wrapTabPanel(page, mMins.get());
            }
        }
        // Re-fit when the active tab changes (commit fires on tab select). Appended, so
        // any XML-defined commit callback of this container is kept. The callback holds its
        // own reference to the stretcher minimums.
        std::shared_ptr<StretcherMins> mins = mMins;
        std::shared_ptr<bool> fit_enabled = mCommitFitEnabled;
        tc->setCommitCallback([tc, mins, fit_enabled](LLUICtrl*, const LLSD&)
        {
            if (*fit_enabled)
            {
                ALScrollFit::fitPage(tc->getCurrentPanel(), mins.get());
            }
        });
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

void Controller::adoptPage(LLPanel* tab_page, LLPanel* content)
{
    if (!tab_page || !content) return;
    ALKitPage* kit_page = dynamic_cast<ALKitPage*>(content);
    if (!kit_page) kit_page = content->findChild<ALKitPage>(tab_page->getName());
    if (!kit_page)
    {
        std::function<ALKitPage*(LLView*)> find_page;
        find_page = [&find_page](LLView* view) -> ALKitPage*
        {
            for (LLView* child : *view->getChildList())
            {
                if (ALKitPage* found = dynamic_cast<ALKitPage*>(child)) return found;
                if (ALKitPage* found = find_page(child)) return found;
            }
            return nullptr;
        };
        kit_page = find_page(content);
    }
    LLScrollContainer* kit_scroller = kit_page && !kit_page->legacy()
        ? kit_page->scroller() : nullptr;
    if (kit_scroller)
    {
        content->reshape(tab_page->getRect().getWidth(), tab_page->getRect().getHeight(), true);
        content->setOrigin(0, 0);
        content->setFollowsAll();
        kit_page->reshape(content->getRect().getWidth(), content->getRect().getHeight(), true);
        kit_page->setOrigin(0, 0);
        kit_page->setFollowsAll();
        if (LLView* doc = scrolledView(kit_scroller)) mFocus.install(kit_scroller, doc, doc);
        enableSliderWheel(content);
        return;
    }

    LLPanel* host = kit_page && kit_page->legacy() ? kit_page->legacyHost() : nullptr;
    if (!host) return;
    if (!hasDirectScrollContainer(host) && std::find(mUnwrapped.begin(), mUnwrapped.end(), host->getName()) == mUnwrapped.end())
    {
        wrapTabPanel(host, mMins.get());
    }
    std::vector<LLScrollContainer*> containers;
    collectDocumentContainers(host, containers);
    for (LLScrollContainer* sc : containers)
    {
        if (LLView* doc = scrolledView(sc)) mFocus.install(sc, doc, doc);
    }
    fitPage(tab_page);
    enableSliderWheel(content);
}
} // namespace ALScrollFit
