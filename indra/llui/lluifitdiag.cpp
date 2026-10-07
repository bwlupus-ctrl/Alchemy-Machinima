/**
 * @file lluifitdiag.cpp
 * @brief One-shot development diagnostics for clipped floater content.
 */

#include "linden_common.h"

#include "lluifitdiag.h"

#include "llcontrol.h"
#include "llfloater.h"
#include "llpanel.h"
#include "llscrollcontainer.h"
#include "llui.h"

namespace LLUIFitDiag
{
namespace
{
struct Overflow
{
    bool horizontal = false;
    bool vertical = false;
};

Overflow childOverflow(const LLView* view)
{
    Overflow result;
    if (!view)
    {
        return result;
    }

    const LLRect local = view->getLocalRect();
    for (const LLView* child : *view->getChildList())
    {
        if (!child)
        {
            continue;
        }
        const LLRect& rect = child->getRect();
        result.horizontal = result.horizontal || rect.mLeft < local.mLeft || rect.mRight > local.mRight;
        result.vertical = result.vertical || rect.mBottom < local.mBottom || rect.mTop > local.mTop;
    }
    return result;
}

bool hasScrollableAncestor(const LLView* view, bool horizontal)
{
    for (LLView* ancestor = view ? view->getParent() : nullptr;
         ancestor;
         ancestor = ancestor->getParent())
    {
        LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(ancestor);
        if (!scroller || !scroller->getScrolledView())
        {
            continue;
        }
        const LLRect document = scroller->getScrolledViewRect();
        const LLRect viewport = scroller->getContentWindowRect();
        if (horizontal ? document.getWidth() > viewport.getWidth()
                       : document.getHeight() > viewport.getHeight())
        {
            return true;
        }
    }
    return false;
}

void walk(LLView* view, const std::string& floater_name)
{
    if (!view)
    {
        return;
    }

    if (LLPanel* panel = dynamic_cast<LLPanel*>(view))
    {
        const Overflow overflow = childOverflow(panel);
        const bool bad_horizontal = overflow.horizontal && !hasScrollableAncestor(panel, true);
        const bool bad_vertical = overflow.vertical && !hasScrollableAncestor(panel, false);
        if (bad_horizontal || bad_vertical)
        {
            LL_WARNS("UIFit") << "Floater '" << floater_name << "': panel '"
                << panel->getName() << "' children exceed its "
                << (bad_horizontal && bad_vertical ? "width and height"
                    : bad_horizontal ? "width" : "height")
                << " without an enclosing scroll container able to scroll that axis"
                << LL_ENDL;
        }
    }

    if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(view))
    {
        LLPanel* document = dynamic_cast<LLPanel*>(scroller->getScrolledView());
        const Overflow overflow = childOverflow(document);
        if (overflow.horizontal || overflow.vertical)
        {
            LL_WARNS("UIFit") << "Floater '" << floater_name
                << "': scroll_container '" << scroller->getName() << "' content '"
                << (document ? document->getName() : std::string("<none>"))
                << "' is smaller than its children on "
                << (overflow.horizontal && overflow.vertical ? "width and height"
                    : overflow.horizontal ? "width" : "height")
                << " (not auto-fit)" << LL_ENDL;
        }
    }

    for (LLView* child : *view->getChildList())
    {
        walk(child, floater_name);
    }
}
} // namespace

void diagnoseFloater(LLFloater* floater)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLControlGroup* config = LLUI::instance().mSettingGroups["config"];
    if (!floater || !config || !config->controlExists("UIFitDiagnostics") ||
        !config->getBOOL("UIFitDiagnostics"))
    {
        return;
    }
    walk(floater, floater->getName());
}
} // namespace LLUIFitDiag
