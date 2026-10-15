#include "llviewerprecompiledheaders.h"

#include "alkitpopover.h"

#include "alkitshell.h"
#include "alkitrow.h"
#include "alkitstyle.h"
#include "llbutton.h"
#include "lltextbox.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llui.h"

#include <vector>

static LLDefaultChildRegistry::Register<ALKitPopover> register_popover("kit_popover");

namespace
{
// LLView::applyXUILayout() gives every XUI widget that declares no height a 10 px
// stand-in (MIN_WIDGET_HEIGHT), never 0 or 1. A popover or child at or below this
// height therefore has no real size of its own.
constexpr S32 kUnsizedHeight = 10;
constexpr S32 kGap           = 4;  // vertical gap between stacked children
constexpr S32 kMargin        = 10; // inset on every side
constexpr S32 kButtonHeight  = 24; // one standard kit button height
constexpr S32 kTextSlack     = 6;  // padding added to a measured text block

// Sizes a popover declared in XUI without a rect: the children stack top-down in
// XML order, each as wide as the popover's interior, and the popover is made exactly
// as tall as the stack (capped to max_height). Word-wrapped text is measured at the
// interior width, so it always has the room its wrapped lines need.
void layoutStack(LLPanel* popover, S32 width, S32 max_height)
{
    const S32 inner = width - 2 * kMargin;
    // LLView::addChild() pushes to the front, so the child list runs in reverse XML
    // order; walk it backwards so the first XML row sits on top.
    std::vector<LLView*> children;
    for (auto it = popover->getChildList()->rbegin(); it != popover->getChildList()->rend(); ++it)
        children.push_back(*it);

    std::vector<S32> heights(children.size(), 0);
    LLTextBox*       last_text = nullptr;
    S32              last_text_index = -1;
    S32              total = 2 * kMargin + (children.empty() ? 0 : kGap * (S32(children.size()) - 1));
    for (size_t i = 0; i < children.size(); ++i)
    {
        LLView* child = children[i];
        S32     child_height;
        if (ALKitRow* row = dynamic_cast<ALKitRow*>(child))
        {
            child_height = row->heightForWidth(inner);
        }
        else if (LLTextBox* text = dynamic_cast<LLTextBox*>(child))
        {
            // Give the block room first so the measure is of the wrapped text, not of
            // a clipped one.
            text->reshape(inner, 4096, true);
            child_height    = llmax(kButtonHeight, text->getTextPixelHeight() + kTextSlack);
            last_text       = text;
            last_text_index = S32(i);
        }
        else if (dynamic_cast<LLButton*>(child) || child->getRect().getHeight() <= kUnsizedHeight)
        {
            child_height = kButtonHeight;
        }
        else
        {
            child_height = child->getRect().getHeight();
        }
        heights[i] = child_height;
        total += child_height;
    }
    // Over the cap: the last text block gives up the excess (its text clips) rather
    // than letting the stack spill outside the popover.
    if (total > max_height && last_text)
    {
        const S32 shrunk = llmax(40, heights[last_text_index] - (total - max_height));
        total -= heights[last_text_index] - shrunk;
        heights[last_text_index] = shrunk;
    }
    const S32 height = llmin(total, max_height);
    popover->reshape(width, height, true);

    S32 top = height - kMargin;
    for (size_t i = 0; i < children.size(); ++i)
    {
        LLView* child = children[i];
        if (ALKitRow* row = dynamic_cast<ALKitRow*>(child))
        {
            row->layoutRow(inner);
        }
        else
        {
            child->reshape(inner, heights[i], true);
        }
        child->setOrigin(kMargin, top - child->getRect().getHeight());
        top -= child->getRect().getHeight() + kGap;
    }
}
} // namespace

ALKitPopover::Params::Params() : anchor("anchor"), side("side", "below")
{
}

ALKitPopover::ALKitPopover(const Params& params) :
    LLPanel(params),
    mAnchorName(params.anchor),
    mSide(params.side),
    mOuterImage(LLUI::getUIImage("Kit_RoundRect6")),
    mInnerImage(LLUI::getUIImage("Kit_RoundRect6"))
{
}

bool ALKitPopover::postBuild()
{
    ALKitShell* shell = ALKitShell::find(this);
    if (shell && !mAnchorName.empty())
    {
        if (LLView* anchor = shell->findChild<LLView>(mAnchorName))
        {
            mAnchor = anchor->getHandle();
        }
    }
    // Style first: the text blocks are measured below, and the kit font decides how
    // many wrapped lines they take.
    const bool built = LLPanel::postBuild();
    ALKitStyle::styleTree(this);
    if (getRect().getHeight() <= kUnsizedHeight)
    {
        S32 width = 360;
        if (getName() == "legend_panel")
            width = 520;
        else if (getName() == "kit_scene_popover")
            width = 180;
        else if (getName() == "kit_hotkeys_popover")
            width = 340;
        const S32 cap = shell ? llmax(120, shell->getRect().getHeight() - 2 * kGap) : 600;
        layoutStack(this, width, cap);
    }
    if (LLUICtrl* close = findChild<LLUICtrl>("btn_legend_close"))
    {
        LLHandle<ALKitPopover> handle = getDerivedHandle<ALKitPopover>();
        close->setCommitCallback(
            [handle](LLUICtrl*, const LLSD&)
            {
                if (ALKitPopover* popover = handle.get())
                    popover->hide();
            });
    }
    return built;
}

void ALKitPopover::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (mOuterImage && mInnerImage)
    {
        mOuterImage->draw(0, 0, getRect().getWidth(), getRect().getHeight(), ALKitStyle::color(ALKitStyle::INPUT_BORDER).get());
        mInnerImage->draw(1, 1, llmax(1, getRect().getWidth() - 2), llmax(1, getRect().getHeight() - 2),
                          ALKitStyle::color(ALKitStyle::RAIL_BG).get());
    }
    LLPanel::draw();
}

void ALKitPopover::show()
{
    ALKitShell* shell  = ALKitShell::find(this);
    LLView*     anchor = mAnchor.get();
    if (!shell || !anchor)
    {
        return;
    }

    shell->closePopover();
    LLRect anchor_rect;
    if (!anchor->localRectToOtherView(anchor->getLocalRect(), &anchor_rect, shell))
    {
        return;
    }

    const S32 width  = getRect().getWidth();
    const S32 height = getRect().getHeight();
    S32       left   = anchor_rect.mLeft;
    S32       bottom = mSide == "above" ? anchor_rect.mTop + 4 : anchor_rect.mBottom - height - 4;
    left             = llclamp(left, 0, llmax(0, shell->getRect().getWidth() - width));
    bottom           = llclamp(bottom, 0, llmax(0, shell->getRect().getHeight() - height));
    setOrigin(left, bottom);
    // The shell's help column, tab strip and rail are added after the XML popovers,
    // so they sit in front of them; a popover that opens over that chrome (the legend
    // anchors at the far right) must be raised or it is hidden behind it.
    if (getParent() == shell)
        shell->sendChildToFront(this);
    setVisible(true);
    shell->setOpenPopover(this);
}

void ALKitPopover::hide()
{ setVisible(false); }

void ALKitPopover::setVisible(bool visible)
{
    LLPanel::setVisible(visible);
    // Every hide path (hide(), Esc, the close button, a direct setVisible(false))
    // ends here, so the shell's open-popover tracking can never go stale.
    if (!visible)
    {
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->popoverHidden(this);
    }
}

void ALKitPopover::toggle()
{
    if (getVisible())
    {
        hide();
    }
    else
    {
        show();
    }
}

void ALKitPopover::bindAnchor(LLView* anchor)
{ mAnchor = anchor ? anchor->getHandle() : LLHandle<LLView>(); }

bool ALKitPopover::handleKeyHere(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE && mask == MASK_NONE)
    {
        hide();
        return true;
    }
    return LLPanel::handleKeyHere(key, mask);
}
