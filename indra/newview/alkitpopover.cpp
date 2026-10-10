#include "llviewerprecompiledheaders.h"

#include "alkitpopover.h"

#include "alkitshell.h"
#include "alkitrow.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"

static LLDefaultChildRegistry::Register<ALKitPopover> register_popover("kit_popover");

ALKitPopover::Params::Params()
    : anchor("anchor"),
      side("side", "below")
{
}

ALKitPopover::ALKitPopover(const Params& params)
    : LLPanel(params),
      mAnchorName(params.anchor),
      mSide(params.side)
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
            if(LLUICtrl* control=dynamic_cast<LLUICtrl*>(anchor))
            {
                LLHandle<ALKitPopover> handle=getDerivedHandle<ALKitPopover>();
                control->setCommitCallback([handle](LLUICtrl*,const LLSD&)
                {
                    if(ALKitPopover* popover=handle.get())popover->show();
                });
            }
        }
    }
    if(getRect().getWidth()<=1||getRect().getHeight()<=1)
    {
        S32 width=360;
        S32 height=180;
        if(getName()=="legend_panel")
        {
            width=520;
            height=420;
        }
        else if(getName()=="kit_hotkeys_popover")
        {
            width=340;
            height=300;
        }
        reshape(width,height,true);
        S32 top=height-10;
        for(LLView* child:*getChildList())
        {
            S32 child_height=child->getRect().getHeight();
            if(child_height<=1)
            {
                if(dynamic_cast<ALKitRow*>(child))child_height=28;
                else if(child->getName()=="btn_legend_close")child_height=24;
                else child_height=height-50;
            }
            child->reshape(width-20,child_height,true);
            child->setOrigin(10,top-child_height);
            top-=child_height+4;
        }
    }
    if(LLUICtrl* close=findChild<LLUICtrl>("btn_legend_close"))
    {
        LLHandle<ALKitPopover> handle=getDerivedHandle<ALKitPopover>();
        close->setCommitCallback([handle](LLUICtrl*,const LLSD&)
        {
            if(ALKitPopover* popover=handle.get())popover->hide();
        });
    }
    return LLPanel::postBuild();
}

void ALKitPopover::show()
{
    ALKitShell* shell = ALKitShell::find(this);
    LLView* anchor = mAnchor.get();
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

    const S32 width = getRect().getWidth();
    const S32 height = getRect().getHeight();
    S32 left = anchor_rect.mLeft;
    S32 bottom = mSide == "above" ? anchor_rect.mTop + 4 : anchor_rect.mBottom - height - 4;
    left = llclamp(left, 0, llmax(0, shell->getRect().getWidth() - width));
    bottom = llclamp(bottom, 0, llmax(0, shell->getRect().getHeight() - height));
    setOrigin(left, bottom);
    setVisible(true);
    shell->setOpenPopover(this);
}

void ALKitPopover::hide()
{
    setVisible(false);
}

void ALKitPopover::bindAnchor(LLView* anchor)
{
    mAnchor=anchor?anchor->getHandle():LLHandle<LLView>();
}

bool ALKitPopover::handleKeyHere(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE && mask == MASK_NONE)
    {
        hide();
        return true;
    }
    return LLPanel::handleKeyHere(key, mask);
}
