#include "llviewerprecompiledheaders.h"

#include "alkitpopover.h"

#include "alkitshell.h"
#include "alkitrow.h"
#include "alkitstyle.h"
#include "llbutton.h"
#include "llscrollcontainer.h"
#include "lltextbox.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llui.h"
#include "llviewercontrol.h"

#include <vector>

static LLDefaultChildRegistry::Register<ALKitPopover> register_popover("kit_popover");

namespace
{
// LLView::applyXUILayout() gives every XUI widget that declares no height a 10 px
// stand-in (MIN_WIDGET_HEIGHT), never 0 or 1. Only a button at or below this height is
// treated as unsized; every other child keeps the height it declares (spacers, dividers).
constexpr S32 kUnsizedHeight = 10;
constexpr S32 kGap           = 4;  // vertical gap between stacked children
constexpr S32 kMargin        = 10; // inset on every side
constexpr S32 kButtonHeight  = 24; // one standard kit button height
constexpr S32 kTextSlack     = 6;  // padding added to a measured text block
constexpr S32 kMinContent    = 40; // least visible height of the scrolling text area

// Children in XML order. LLView::addChild() pushes to the front, so the child list runs
// in reverse XML order; walking it backwards yields the XML order.
std::vector<LLView*> xmlOrder(LLView* parent)
{
    std::vector<LLView*> children;
    for (auto it = parent->getChildList()->rbegin(); it != parent->getChildList()->rend(); ++it)
        children.push_back(*it);
    return children;
}

// The height a child takes in a stack laid out at `width`.
S32 naturalHeight(LLView* child, S32 width)
{
    if (ALKitRow* row = dynamic_cast<ALKitRow*>(child))
        return row->heightForWidth(width);
    if (LLTextBox* text = dynamic_cast<LLTextBox*>(child))
    {
        // Give the block room first so the measure is of the wrapped text, not of a
        // clipped one.
        text->reshape(width, 4096, true);
        return llmax(kButtonHeight, text->getTextPixelHeight() + kTextSlack);
    }
    if (dynamic_cast<LLButton*>(child) && child->getRect().getHeight() <= kUnsizedHeight)
        return kButtonHeight;
    return child->getRect().getHeight();
}

// Fills `heights` for `kids` stacked at `width` and returns the stack's total height,
// gaps included.
S32 stackHeights(const std::vector<LLView*>& kids, S32 width, std::vector<S32>& heights)
{
    heights.clear();
    S32 total = kids.empty() ? 0 : kGap * (S32(kids.size()) - 1);
    for (LLView* child : kids)
    {
        heights.push_back(naturalHeight(child, width));
        total += heights.back();
    }
    return total;
}

// Places `kids` top-down, starting with their top edge at `top`, at x = `left`, each
// `width` wide.
void placeStack(const std::vector<LLView*>& kids, const std::vector<S32>& heights, S32 left, S32 width, S32 top)
{
    for (size_t i = 0; i < kids.size(); ++i)
    {
        LLView* child = kids[i];
        if (ALKitRow* row = dynamic_cast<ALKitRow*>(child))
            row->layoutRow(width);
        else
            child->reshape(width, heights[i], true);
        child->setOrigin(left, top - child->getRect().getHeight());
        top -= child->getRect().getHeight() + kGap;
    }
}

// The tallest an auto-sized popover may be: the shell's current height less a margin.
S32 popoverCap(const ALKitShell* shell)
{
    return shell ? llmax(120, shell->getRect().getHeight() - 2 * kGap) : 600;
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
        mAutoWidth = 360;
        if (getName() == "legend_panel")
            mAutoWidth = 520;
        else if (getName() == "kit_scene_popover")
            mAutoWidth = 180;
        else if (getName() == "kit_hotkeys_popover")
            mAutoWidth = 340;
        else if (getName() == "kit_overflow_popover")
            mAutoWidth = 180;
        layoutPopover(mAutoWidth, popoverCap(shell), true);
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

// Moves every child that is not a button into the scrolling document, so the text and
// rows can scroll while the buttons stay pinned. Idempotent; a no-op when there is no
// such child.
void ALKitPopover::ensureScroller()
{
    std::vector<LLView*> stray;
    for (LLView* child : xmlOrder(this))
        if (child != mScroller && !dynamic_cast<LLButton*>(child))
            stray.push_back(child);
    if (stray.empty())
        return;

    if (!mScroller)
    {
        LLScrollContainer::Params sp;
        sp.name                  = getName() + "_scroll";
        sp.rect                  = getLocalRect();
        sp.is_opaque             = false;
        sp.reserve_scroll_corner = false;
        mScroller                = LLUICtrlFactory::create<LLScrollContainer>(sp);
        ALKitStyle::styleScroller(mScroller);

        LLPanel::Params dp;
        dp.name               = getName() + "_content";
        dp.rect               = getLocalRect();
        dp.background_visible = false;
        mDoc                  = LLUICtrlFactory::create<LLPanel>(dp);
        mScroller->addChild(mDoc); // the first child becomes the scrolled view
        addChild(mScroller);
    }
    for (LLView* child : stray)
        mDoc->addChild(child); // removes it from this popover
}

// Sizes an auto-sized popover: the content stacks top-down in the scrolling area, the
// buttons stay pinned under it, and the popover is as tall as its natural height, capped
// to max_height. A content stack taller than the room left by the buttons scrolls.
// to_top (show) starts at the top; a refit keeps the scroll position, clamped to the new range.
void ALKitPopover::layoutPopover(S32 width, S32 max_height, bool to_top)
{
    const S32 kept_scroll = mScroller ? mScroller->getDocPosVertical() : 0;
    ensureScroller();
    const S32 inner = width - 2 * kMargin;
    const S32 sb    = llmax(0, gSavedSettings.getS32("UIScrollbarSize"));

    std::vector<LLView*> content = mDoc ? xmlOrder(mDoc) : std::vector<LLView*>();
    std::vector<LLView*> buttons;
    for (LLView* child : xmlOrder(this))
        if (child != mScroller)
            buttons.push_back(child);

    std::vector<S32> button_heights;
    std::vector<S32> content_heights;
    const S32        button_block      = stackHeights(buttons, inner, button_heights);
    const S32        gap_above_buttons = (!content.empty() && !buttons.empty()) ? kGap : 0;
    const S32        fixed             = 2 * kMargin + button_block + gap_above_buttons;

    // The text area keeps at least kMinContent px, so the buttons never leave the popover
    // even when the cap is tiny.
    S32       content_h  = stackHeights(content, inner, content_heights);
    const S32 min_height = fixed + (content.empty() ? 0 : kMinContent);
    const S32 height     = llmax(min_height, llmin(fixed + content_h, max_height));
    const S32 scroll_h   = height - fixed;
    S32       doc_w      = inner;
    if (content_h > scroll_h)
    {
        // Overflows: the scrollbar takes its width, so measure again at the narrower width.
        doc_w     = inner - sb;
        content_h = stackHeights(content, doc_w, content_heights);
    }

    reshape(width, height, true);
    placeStack(buttons, button_heights, kMargin, inner, kMargin + button_block);

    if (mScroller)
    {
        const S32 scroll_bottom = kMargin + button_block + gap_above_buttons;
        mScroller->reshape(inner, scroll_h, true);
        mScroller->setOrigin(kMargin, scroll_bottom);
        // The document is never shorter than the visible area, so short content sits at
        // the top rather than floating in the middle.
        const S32 doc_h = llmax(content_h, scroll_h);
        mDoc->reshape(doc_w, doc_h, true);
        placeStack(content, content_heights, 0, doc_w, doc_h);
        mScroller->reshape(inner, scroll_h, true);
        if (to_top)
            mScroller->goToTop();
        else
            mScroller->setDocPosVertical(kept_scroll); // the scrollbar clamps it to the new range
    }
}

// Caps a fixed-size popover to the room the shell has and scrolls what no longer fits.
// The designed size is remembered on first use, so a later, taller shell restores it.
void ALKitPopover::fitFixedHeight(S32 max_height, bool to_top)
{
    if (mAutoWidth > 0)
        return;
    const S32 kept_scroll = mScroller ? mScroller->getDocPosVertical() : 0;
    if (mFixedHeight <= 0)
    {
        mFixedWidth  = getRect().getWidth();
        mFixedHeight = getRect().getHeight();
    }
    const S32 shown = llmin(mFixedHeight, max_height);
    if (shown >= mFixedHeight && !mScroller)
        return;
    if (!mScroller)
    {
        LLScrollContainer::Params sp;
        sp.name                  = getName() + "_fixed_scroll";
        sp.rect                  = LLRect(0, mFixedHeight, mFixedWidth, 0);
        sp.is_opaque             = false;
        sp.reserve_scroll_corner = false;
        mScroller                = LLUICtrlFactory::create<LLScrollContainer>(sp);
        ALKitStyle::styleScroller(mScroller);

        LLPanel::Params dp;
        dp.name               = getName() + "_fixed_content";
        dp.rect               = LLRect(0, mFixedHeight, mFixedWidth, 0);
        dp.background_visible = false;
        mDoc                  = LLUICtrlFactory::create<LLPanel>(dp);
        std::vector<LLView*> kids;
        for (LLView* child : *getChildList())
            kids.push_back(child);
        for (LLView* child : kids)
            mDoc->addChild(child); // removes it from this popover; positions are unchanged
        mScroller->addChild(mDoc);
        addChild(mScroller);
    }
    const bool overflow = shown < mFixedHeight;
    const S32  sb       = overflow ? llmax(0, gSavedSettings.getS32("UIScrollbarSize")) : 0;
    reshape(mFixedWidth + sb, shown, true); // called_from_parent: never notify the shell (no reshape loop)
    mScroller->reshape(mFixedWidth + sb, shown, true);
    mScroller->setOrigin(0, 0);
    mDoc->reshape(mFixedWidth, mFixedHeight, false);
    if (to_top)
        mScroller->goToTop();
    else
        mScroller->setDocPosVertical(kept_scroll); // the scrollbar clamps it to the new range
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
    ALKitShell* shell = ALKitShell::find(this);
    if (!shell || !mAnchor.get())
    {
        return;
    }
    shell->closePopover();
    if (!place(true))
    {
        return;
    }
    // The shell's help column, tab strip and rail are added after the XML popovers,
    // so they sit in front of them; a popover that opens over that chrome (the legend
    // anchors at the far right) must be raised or it is hidden behind it.
    if (getParent() == shell)
        shell->sendChildToFront(this);
    setVisible(true);
    shell->setOpenPopover(this);
}

void ALKitPopover::refit()
{
    // Called by the shell when it reshapes while this popover is open: re-fit to the
    // new height and re-clamp the position so it cannot end up outside the shell.
    if (getVisible())
        place(false);
}

bool ALKitPopover::place(bool to_top)
{
    ALKitShell* shell  = ALKitShell::find(this);
    LLView*     anchor = mAnchor.get();
    if (!shell || !anchor)
    {
        return false;
    }
    // A collapsed bottom-bar anchor is hidden and stale; open from the bar's "..." button.
    if (!anchor->getVisible())
        if (LLView* more = shell->findChild<LLView>("btn_bar_more"))
            if (more->getVisible())
                anchor = more;

    // Re-fit an auto-sized popover to the shell's current height: a resized or
    // minimum-size Director may be shorter than it was when the XML was built.
    if (mAutoWidth > 0)
    {
        layoutPopover(mAutoWidth, popoverCap(shell), to_top);
    }
    else
    {
        fitFixedHeight(popoverCap(shell), to_top);
    }
    LLRect anchor_rect;
    if (!anchor->localRectToOtherView(anchor->getLocalRect(), &anchor_rect, shell))
    {
        return false;
    }

    const S32 width  = getRect().getWidth();
    const S32 height = getRect().getHeight();
    S32       left   = anchor_rect.mLeft;
    S32       bottom = mSide == "above" ? anchor_rect.mTop + 4 : anchor_rect.mBottom - height - 4;
    left             = llclamp(left, 0, llmax(0, shell->getRect().getWidth() - width));
    bottom           = llclamp(bottom, 0, llmax(0, shell->getRect().getHeight() - height));
    setOrigin(left, bottom);
    return true;
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

bool ALKitPopover::anchorContains(S32 x, S32 y) const
{
    LLView*     anchor = mAnchor.get();
    ALKitShell* shell  = ALKitShell::find(const_cast<ALKitPopover*>(this));
    if (!anchor || !shell || !anchor->getVisible())
        return false;
    LLRect rect;
    if (!anchor->localRectToOtherView(anchor->getLocalRect(), &rect, shell))
        return false;
    return rect.pointInRect(x, y);
}

void ALKitPopover::toggle()
{
    // The press that landed on this popover's own anchor already closed it (the
    // shell's outside-click handler); the click's commit must not reopen it.
    if (ALKitShell* shell = ALKitShell::find(this))
        if (shell->consumeAnchorClose(this))
            return;
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
