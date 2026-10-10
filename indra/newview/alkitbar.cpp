#include "llviewerprecompiledheaders.h"

#include "alkitbar.h"

#include "llstring.h"
#include "lluictrlfactory.h"

static LLDefaultChildRegistry::Register<ALKitBar> register_bar("kit_bar");

ALKitBar::Params::Params()
    : flex("flex"),
      collapse_order("collapse_order")
{
}

ALKitBar::ALKitBar(const Params& params)
    : LLPanel(params),
      mFlexName(params.flex)
{
    LLStringUtil::getTokens(params.collapse_order, mCollapseOrder, ",");
}

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
    for (const std::string& name : mCollapseOrder)
    {
        if (LLView* child = findChild<LLView>(name))
        {
            mCollapse.push_back(child);
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

void ALKitBar::layoutChildren()
{
    constexpr S32 PAD = 10;
    constexpr S32 GAP = 8;
    for (LLView* child : mCollapse)
    {
        child->setVisible(true);
    }

    auto required_width = [this]()
    {
        S32 width = 20;
        S32 count = 0;
        for (LLView* child : mChildren)
        {
            if (child != mFlex && child->getVisible())
            {
                width += child->getRect().getWidth();
                ++count;
            }
        }
        return width + llmax(0, count) * 8;
    };

    for (LLView* child : mCollapse)
    {
        if (required_width() <= getRect().getWidth())
        {
            break;
        }
        child->setVisible(false);
    }

    S32 fixed = 0;
    S32 visible = 0;
    for (LLView* child : mChildren)
    {
        if (child != mFlex && child->getVisible())
        {
            fixed += child->getRect().getWidth();
            ++visible;
        }
    }
    const S32 flex_width = mFlex ? llmax(0, getRect().getWidth() - 2 * PAD - fixed - visible * GAP) : 0;
    S32 left = PAD;
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
