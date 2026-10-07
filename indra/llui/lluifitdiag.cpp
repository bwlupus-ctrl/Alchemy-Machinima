/**
 * @file lluifitdiag.cpp
 * @brief One-shot development diagnostics for clipped floater content.
 */

#include "linden_common.h"

#include "lluifitdiag.h"

#include "llcontrol.h"
#include "llfloater.h"
#include "lllayoutstack.h"
#include "llpanel.h"
#include "llscrollcontainer.h"
#include "llstring.h"
#include "llui.h"

#include <set>
#include <vector>

namespace LLUIFitDiag
{
namespace
{
struct Overflow
{
    bool horizontal = false;
    bool vertical = false;
};

struct IgnorePattern
{
    std::string floater;
    std::string panel;
    bool horizontal = true;
    bool vertical = true;
};

struct WalkResult
{
    S32 issues = 0;
    bool deferred = false;
};

Overflow childOverflow(const LLView* view, S32 tolerance)
{
    Overflow result;
    if (!view)
    {
        return result;
    }

    const LLRect local = view->getLocalRect();
    for (const LLView* child : *view->getChildList())
    {
        // [UIFitNoise] A hidden child cannot clip its parent. The tree walk still
        // diagnoses that child's own descendants, including hidden tab/mode pages.
        if (!child || !child->getVisible())
        {
            continue;
        }
        const LLRect& rect = child->getRect();
        result.horizontal = result.horizontal ||
            rect.mLeft < local.mLeft - tolerance || rect.mRight > local.mRight + tolerance;
        result.vertical = result.vertical ||
            rect.mBottom < local.mBottom - tolerance || rect.mTop > local.mTop + tolerance;
    }
    return result;
}

bool hasVisibleChildren(const LLView* view)
{
    if (view)
    {
        for (const LLView* child : *view->getChildList())
        {
            if (child && child->getVisible())
            {
                return true;
            }
        }
    }
    return false;
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

// [UIFitNoise] Parse dynamic direct-child exceptions once per diagnostic pass.
std::vector<IgnorePattern> parseIgnorePatterns(LLControlGroup* config)
{
    std::vector<IgnorePattern> result;
    if (!config || !config->controlExists("UIFitDiagnosticsIgnore"))
    {
        return result;
    }

    const std::vector<std::string> patterns =
        LLStringUtil::getTokens(config->getString("UIFitDiagnosticsIgnore"), ",");
    for (std::string pattern : patterns)
    {
        LLStringUtil::trim(pattern);
        const std::string::size_type slash = pattern.find('/');
        if (slash == std::string::npos || pattern.find('/', slash + 1) != std::string::npos)
        {
            continue;
        }

        IgnorePattern parsed;
        parsed.floater = pattern.substr(0, slash);
        parsed.panel = pattern.substr(slash + 1);
        if (parsed.panel.size() > 2 && parsed.panel[parsed.panel.size() - 2] == ':')
        {
            const char axis = parsed.panel.back();
            if (axis == 'h' || axis == 'v')
            {
                parsed.panel.erase(parsed.panel.size() - 2);
                parsed.horizontal = axis == 'h';
                parsed.vertical = axis == 'v';
            }
        }

        // [UIFitNoise] Reject malformed entries, especially a broad empty "*/" panel.
        if (!parsed.floater.empty() && !parsed.panel.empty())
        {
            result.push_back(parsed);
        }
    }
    return result;
}

Overflow ignoredAxes(const LLPanel* panel, const std::string& floater_name,
                     const std::vector<IgnorePattern>& patterns)
{
    Overflow result;
    for (const IgnorePattern& pattern : patterns)
    {
        if ((pattern.floater == "*" || pattern.floater == floater_name) &&
            pattern.panel == panel->getName())
        {
            result.horizontal = result.horizontal || pattern.horizontal;
            result.vertical = result.vertical || pattern.vertical;
        }
    }
    return result;
}

// [UIFitNoise] Detect only actual layout-stack visibility/collapse motion. In
// steady state the clip is exactly the collapsed minimum or expanded target.
bool layoutPanelIsAnimating(const LLLayoutPanel* panel)
{
    if (!panel || !dynamic_cast<const LLLayoutStack*>(panel->getParent()))
    {
        return false;
    }

    const F32 visible_amount = panel->getVisibleAmount();
    if (visible_amount > 0.f && visible_amount < 1.f)
    {
        return true;
    }
    // [UIFitNoise] getVisibleDim() uses the raw relevant min (may be -1 when no min_dim).
    return visible_amount == 1.f && panel->getVisibleDim() !=
        (panel->isCollapsed() ? panel->getRelevantMinDim() : panel->getTargetDim());
}

// [UIFitNoise] De-duplicate each axis by its full branch path across the session.
Overflow takeUnreportedAxes(const std::string& floater_name,
                            const std::string& view_path,
                            const Overflow& overflow)
{
    if (!overflow.horizontal && !overflow.vertical)
    {
        return Overflow();
    }

    static std::set<std::string> reported;
    const std::string prefix = floater_name + '\x1f' + view_path + '\x1f';
    Overflow result;
    if (overflow.horizontal)
    {
        result.horizontal = reported.insert(prefix + "horizontal").second;
    }
    if (overflow.vertical)
    {
        result.vertical = reported.insert(prefix + "vertical").second;
    }
    return result;
}

WalkResult walk(LLView* view, const std::string& floater_name,
                const std::string& view_path,
                const std::vector<IgnorePattern>& ignore_patterns,
                S32 tolerance,
                bool under_collapsed = false)
{
    WalkResult result;
    if (!view)
    {
        return result;
    }

    // [UIFitNoise] Track whether this node is under a collapsed layout panel.
    bool child_under_collapsed = under_collapsed;

    if (LLPanel* panel = dynamic_cast<LLPanel*>(view))
    {
        const LLRect& rect = panel->getRect();
        const LLLayoutPanel* layout_panel = dynamic_cast<LLLayoutPanel*>(panel);
        const bool stack_owned = layout_panel &&
            dynamic_cast<const LLLayoutStack*>(layout_panel->getParent());
        const bool transient = layoutPanelIsAnimating(layout_panel);
        if (transient)
        {
            // [UIFitNoise] Keep walking descendants, but retry after motion stops.
            result.deferred = true;
        }

        // [UIFitNoise] Only a layout stack may make a panel's own boundary
        // intentionally unusable. Never prune its descendants from the walk.
        const bool stack_suppressed = stack_owned &&
            (layout_panel->isCollapsed() || !layout_panel->getVisible());
        // [UIFitNoise] Update child_under_collapsed for descendants.
        child_under_collapsed = under_collapsed || stack_suppressed;
        const bool zero_size = rect.getWidth() <= 0 || rect.getHeight() <= 0;
        // [UIFitNoise] Suppress zero-size warnings for children of collapsed panels.
        if (!transient && !stack_suppressed && !under_collapsed && zero_size && hasVisibleChildren(panel))
        {
            const Overflow zero_axes = { rect.getWidth() <= 0, rect.getHeight() <= 0 };
            const Overflow unreported = takeUnreportedAxes(floater_name, "zero:" + view_path, zero_axes);
            if (unreported.horizontal || unreported.vertical)
            {
                LL_WARNS("UIFit") << "Floater '" << floater_name << "': panel '"
                    << panel->getName() << "' is a zero-size container with visible children"
                    << LL_ENDL;
                ++result.issues;
            }
        }
        else if (!transient && !stack_suppressed && !zero_size)
        {
            Overflow overflow = childOverflow(panel, tolerance);
            const Overflow ignored = ignoredAxes(panel, floater_name, ignore_patterns);
            overflow.horizontal = overflow.horizontal && !ignored.horizontal;
            overflow.vertical = overflow.vertical && !ignored.vertical;
            const bool bad_horizontal = overflow.horizontal &&
                !hasScrollableAncestor(panel, true);
            const bool bad_vertical = overflow.vertical &&
                !hasScrollableAncestor(panel, false);
            const Overflow unreported = takeUnreportedAxes(
                floater_name, view_path, { bad_horizontal, bad_vertical });
            if (unreported.horizontal || unreported.vertical)
            {
                LL_WARNS("UIFit") << "Floater '" << floater_name << "': panel '"
                    << panel->getName() << "' children exceed its "
                    << (unreported.horizontal && unreported.vertical ? "width and height"
                        : unreported.horizontal ? "width" : "height")
                    << " without an enclosing scroll container able to scroll that axis"
                    << LL_ENDL;
                ++result.issues;
            }
        }
    }

    if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(view))
    {
        LLPanel* document = dynamic_cast<LLPanel*>(scroller->getScrolledView());
        const Overflow overflow = childOverflow(document, tolerance);
        const Overflow unreported = takeUnreportedAxes(
            floater_name, view_path, overflow);
        if (unreported.horizontal || unreported.vertical)
        {
            LL_WARNS("UIFit") << "Floater '" << floater_name
                << "': scroll_container '" << scroller->getName() << "' content '"
                << (document ? document->getName() : std::string("<none>"))
                << "' is smaller than its children on "
                << (unreported.horizontal && unreported.vertical ? "width and height"
                    : unreported.horizontal ? "width" : "height")
                << " (not auto-fit)" << LL_ENDL;
            ++result.issues;
        }
    }

    for (LLView* child : *view->getChildList())
    {
        const std::string child_path = view_path + "/" +
            (child->getName().empty() ? std::string("<unnamed>") : child->getName());
        const WalkResult child_result = walk(
            child, floater_name, child_path, ignore_patterns, tolerance, child_under_collapsed);
        result.issues += child_result.issues;
        result.deferred = result.deferred || child_result.deferred;
    }
    return result;
}
} // namespace

bool diagnoseFloater(LLFloater* floater)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLControlGroup* config = LLUI::instance().mSettingGroups["config"];
    if (!floater || !config || !config->controlExists("UIFitDiagnostics") ||
        !config->getBOOL("UIFitDiagnostics"))
    {
        return true;
    }

    // [UIFitNoise] Parse pass-wide policy once, then carry it through the tree.
    const std::vector<IgnorePattern> ignore_patterns = parseIgnorePatterns(config);
    const S32 tolerance = config->controlExists("UIFitDiagnosticsTolerancePx")
        ? llmax(0, config->getS32("UIFitDiagnosticsTolerancePx")) : 0;
    const WalkResult result = walk(
        floater, floater->getName(), floater->getName(), ignore_patterns, tolerance, false);
    if (result.issues > 0)
    {
        // [UIFitNoise] One concise total makes the remaining actionable output scannable.
        LL_INFOS("UIFit") << "[UIFit] floater '" << floater->getName()
                          << "': " << result.issues << " issue(s)" << LL_ENDL;
    }
    return !result.deferred;
}
} // namespace LLUIFitDiag
