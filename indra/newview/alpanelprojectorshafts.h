/** @file alpanelprojectorshafts.h @brief Shared panel_projector_shafts.xml controls. */
#ifndef AL_ALPANELPROJECTORSHAFTS_H
#define AL_ALPANELPROJECTORSHAFTS_H
#include "llpanel.h"
#include "aluirefresh.h"
class LLTextBox;
class ALPanelProjectorShafts final : public LLPanel
{
public:
    ALPanelProjectorShafts();
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
private:
    LLTextBox* mShaftCost = nullptr;
    LLView* mFlowGrid = nullptr;
    ALLiveReadoutClock mCostClock{0.5f}; // same launcher-clock pattern, every 2 s
};
#endif
