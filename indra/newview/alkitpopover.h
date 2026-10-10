#ifndef AL_KIT_POPOVER_H
#define AL_KIT_POPOVER_H

#include "llpanel.h"

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
    void show();
    void hide();
    void toggle();
    void bindAnchor(LLView* anchor);
    bool handleKeyHere(KEY key, MASK mask) override;

protected:
    friend class LLUICtrlFactory;
    explicit ALKitPopover(const Params& params);

private:
    std::string mAnchorName;
    std::string mSide;
    LLHandle<LLView> mAnchor;
};

#endif
