#include "llviewerprecompiledheaders.h"

#include "alkitbar.h"

#include "llstring.h"
#include "lluictrlfactory.h"

#include <algorithm>

static LLDefaultChildRegistry::Register<ALKitBar> register_bar("kit_bar");

ALKitBar::Params::Params() : flex("flex"), collapse_order("collapse_order")
{
}

ALKitBar::ALKitBar(const Params& params) : LLPanel(params), mFlexName(params.flex)
{ LLStringUtil::getTokens(params.collapse_order, mCollapseOrder, ","); }

bool ALKitBar::postBuild()
{
    for (LLView* child : *getChildList())
    {
        mChildren.push_back(child);
        if (child->getName() == mFlexName)
        {
            mFlex = child;
        }
    }
    // LLView stores children frontmost-first, the reverse of XUI declaration
    // order. Transport semantics and visual order follow the declaration.
    std::reverse(mChildren.begin(), mChildren.end());
    for (const std::string& name : mCollapseOrder)
    {
        auto found = std::find_if(mChildren.begin(), mChildren.end(), [&name](LLView* child) { return child->getName() == name; });
        if (found != mChildren.end())
        {
            LLView* child = *found;
            mCollapse.push_back(child);
            mNaturalVisible.push_back(child->getVisible());
            mCollapsed.push_back(false);
        }
    }
    layoutChildren();
    return LLPanel::postBuild();
}

void ALKitBar::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    layoutChildren();
}

void ALKitBar::setCollapsibleVisible(LLView* child, bool visible)
{
    if (!child)
    {
        return;
    }
    for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
    {
        if (mCollapse[index] == child)
        {
            mNaturalVisible[index] = visible;
            mCollapsed[index]      = false;
            break;
        }
    }
    child->setVisible(visible);
    layoutChildren();
}

void ALKitBar::layoutChildren()
{
    constexpr S32 PAD = 10;
    constexpr S32 GAP = 8;
    constexpr S32 FLEX_MIN = 120;
    for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
    {
        if (!mCollapsed[index])
        {
            mNaturalVisible[index] = mCollapse[index]->getVisible();
        }
        mCollapsed[index] = false;
    }

    auto required_width = [this]()
    {
        S32 width = 20;
        S32 count = 0;
        for (LLView* child : mChildren)
        {
            bool visible = child->getVisible();
            for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
            {
                if (mCollapse[index] == child)
                {
                    visible = mNaturalVisible[index] && !mCollapsed[index];
                    break;
                }
            }
            if (child != mFlex && visible)
            {
                width += child->getRect().getWidth();
                ++count;
            }
        }
        if (mFlex && mFlex->getVisible())
        {
            width += FLEX_MIN;
            ++count;
        }
        return width + llmax(0, count - 1) * GAP;
    };

    for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
    {
        if (required_width() <= getRect().getWidth())
        {
            break;
        }
        if (mNaturalVisible[index])
        {
            mCollapsed[index] = true;
        }
    }
    for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
    {
        const bool visible = mNaturalVisible[index] && !mCollapsed[index];
        if (mCollapse[index]->getVisible() != visible)
        {
            mCollapse[index]->setVisible(visible);
        }
    }

    S32 fixed   = 0;
    S32 visible = 0;
    for (LLView* child : mChildren)
    {
        if (child != mFlex && child->getVisible())
        {
            fixed += child->getRect().getWidth();
            ++visible;
        }
    }
    const S32 flex_width = mFlex && mFlex->getVisible()
                               ? llmax(FLEX_MIN, getRect().getWidth() - 2 * PAD - fixed - visible * GAP)
                               : 0;
    S32       left       = PAD;
    for (LLView* child : mChildren)
    {
        if (!child->getVisible())
        {
            continue;
        }
        const S32 width = child == mFlex ? flex_width : child->getRect().getWidth();
        child->reshape(width, child->getRect().getHeight(), true);
        child->setOrigin(left, (getRect().getHeight() - child->getRect().getHeight()) / 2);
        left += width + GAP;
    }
}
