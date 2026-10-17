#include "llviewerprecompiledheaders.h"

#include "alkitbar.h"

#include "alkitstyle.h"
#include "llbutton.h"
#include "llrender.h"
#include "llstring.h"
#include "llui.h"
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
    // Overflow button: whatever the width collapses stays reachable through it.
    LLButton::Params more;
    more.name     = "btn_bar_more";
    more.label    = "...";
    more.rect     = LLRect(0, 24, 28, 0);
    more.tool_tip = "More controls";
    mMore         = LLUICtrlFactory::create<LLButton>(more);
    mMore->setVisible(false);
    addChild(mMore);
    mChildren.push_back(mMore);
    layoutChildren();
    return LLPanel::postBuild();
}

void ALKitBar::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    layoutChildren();
}

void ALKitBar::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    static LLPointer<LLUIImage> pill = LLUI::getUIImage("Kit_Pill9");
    for (LLView* child : mChildren)
    {
        if (!child || !child->getVisible())
            continue;
        const LLRect& rect = child->getRect();
        const std::string& name = child->getName();
        if (name.rfind("transport_divider_", 0) == 0)
        {
            gl_rect_2d(rect, ALKitStyle::color(ALKitStyle::ARC_TRACK).get(), true);
        }
        else if (name == "rec_indicator" && pill)
        {
            if (mRecording)
            {
                pill->draw(rect, ALKitStyle::color(ALKitStyle::RECORD).get());
            }
            else
            {
                pill->draw(rect, ALKitStyle::color(ALKitStyle::INPUT_BORDER).get());
                LLRect inner(rect.mLeft + 1, rect.mTop - 1, rect.mRight - 1, rect.mBottom + 1);
                pill->draw(inner, ALKitStyle::color(ALKitStyle::RAIL_BG).get());
            }
            gGL.color4fv((mRecording ? LLColor4::white : ALKitStyle::color(ALKitStyle::FAINT).get()).mV);
            gl_circle_2d((F32)rect.mLeft + 9.f, (F32)(rect.mBottom + rect.mTop) * .5f, 3.f, 12, true);
        }
        else if ((name == "btn_scene_load" || name == "btn_scene_save" || name == "btn_scene_menu") && child->getVisible())
        {
            gl_rect_2d(rect, ALKitStyle::color(ALKitStyle::INPUT_BORDER).get(), false);
        }
        else if (name == "status_strip")
        {
            gGL.color4fv(ALKitStyle::color(ALKitStyle::RECORD).get().mV);
            gl_circle_2d((F32)rect.mLeft + 5.f, (F32)(rect.mBottom + rect.mTop) * .5f, 3.5f, 12, true);
        }
        else if (name == "transport_toast" && pill)
        {
            pill->draw(rect, ALKitStyle::color(ALKitStyle::ACCENT).get());
            LLRect inner(rect.mLeft + 1, rect.mTop - 1, rect.mRight - 1, rect.mBottom + 1);
            pill->draw(inner, ALKitStyle::color(ALKitStyle::CONTROL_BG).get());
        }
    }
    LLPanel::draw();
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

std::vector<LLView*> ALKitBar::collapsedChildren() const
{
    std::vector<LLView*> collapsed;
    for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
        if (mCollapsed[index] && mNaturalVisible[index])
            collapsed.push_back(mCollapse[index]);
    return collapsed;
}

// Icon + label buttons (ACTION, CUT) centre the pair as one unit: LLButton
// otherwise centres the label in the space right of a left-aligned icon.
static void centreIconLabel(LLButton* button)
{
    if (!button || !button->getImageOverlay() || button->getImageOverlayHAlign() != LLFontGL::LEFT ||
        !button->getFont())
        return;
    constexpr S32 ICON_LABEL_GAP = 6; // matches imgoverlay_label_space in the generated XUI
    const S32 content = button->getImageOverlay()->getWidth() + ICON_LABEL_GAP +
                        button->getFont()->getWidth(button->getText());
    button->setHAlign(LLFontGL::LEFT);
    button->setLeftHPad(llmax(4, (button->getRect().getWidth() - content) / 2));
}

void ALKitBar::layoutChildren()
{
    for (const char* name : { "btn_action", "btn_cut" })
        centreIconLabel(findChild<LLButton>(name, true));
    constexpr S32 PAD = 10;
    constexpr S32 GAP = 8;
    constexpr S32 FLEX_MIN = 0;
    for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
    {
        if (!mCollapsed[index])
        {
            mNaturalVisible[index] = mCollapse[index]->getVisible();
        }
        mCollapsed[index] = false;
    }

    bool more_counted = false; // the overflow button takes room only while something is collapsed
    auto layout_visible = [this, &more_counted](LLView* child)
    {
        if (child == mMore)
            return more_counted;
        for (S32 index = 0; index < (S32)mCollapse.size(); ++index)
            if (mCollapse[index] == child)
                return mNaturalVisible[index] && !mCollapsed[index];
        return child->getVisible();
    };
    auto required_width = [this, PAD, &layout_visible]()
    {
        S32 width = 2 * PAD;
        S32 count = 0;
        for (LLView* child : mChildren)
        {
            const bool visible = layout_visible(child);
            if (child != mFlex && visible)
            {
                width += child->getRect().getWidth();
                ++count;
            }
        }
        if (mFlex && layout_visible(mFlex))
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
            more_counted      = true;
        }
    }
    if (mMore && mMore->getVisible() != more_counted)
        mMore->setVisible(more_counted);
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
