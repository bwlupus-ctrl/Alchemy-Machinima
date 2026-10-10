#ifndef AL_KIT_BAR_H
#define AL_KIT_BAR_H

#include "llpanel.h"

#include <string>
#include <vector>

class ALKitBar final : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Optional<std::string> flex;
        Optional<std::string> collapse_order;

        Params();
    };

    bool postBuild() override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;

protected:
    friend class LLUICtrlFactory;
    explicit ALKitBar(const Params& params);

private:
    void layoutChildren();

    std::string mFlexName;
    std::vector<std::string> mCollapseOrder;
    LLView* mFlex = nullptr;
    std::vector<LLView*> mChildren;
    std::vector<LLView*> mCollapse;
};

#endif
