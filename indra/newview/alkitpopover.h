#ifndef AL_KIT_POPOVER_H
#define AL_KIT_POPOVER_H

#include "llpanel.h"
#include "lluiimage.h"

class LLScrollContainer;

class ALKitPopover final : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Optional<std::string> anchor;
        Optional<std::string> side;

        Params();
    };

    bool postBuild() override;
    void draw() override;
    void show();
    void hide();
    void toggle();
    void bindAnchor(LLView* anchor);
    bool handleKeyHere(KEY key, MASK mask) override;
    void setVisible(bool visible) override;

protected:
    friend class LLUICtrlFactory;
    explicit ALKitPopover(const Params& params);

private:
    void ensureScroller();
    void layoutPopover(S32 width, S32 max_height);

    std::string        mAnchorName;
    std::string        mSide;
    LLHandle<LLView>   mAnchor;
    LLUIImagePtr       mOuterImage;
    LLUIImagePtr       mInnerImage;
    // Auto-sized popovers (no rect in XML) keep their content in mScroller/mDoc so an
    // over-tall stack scrolls instead of clipping. mAutoWidth > 0 marks an auto-sized
    // popover; show() re-runs the layout against the shell's current height.
    LLScrollContainer* mScroller  = nullptr;
    LLPanel*           mDoc       = nullptr;
    S32                mAutoWidth = 0;
};

#endif
