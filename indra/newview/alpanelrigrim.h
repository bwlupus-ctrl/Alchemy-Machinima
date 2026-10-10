#ifndef AL_PANEL_RIG_RIM_H
#define AL_PANEL_RIG_RIM_H

#include "alrigrimcontroller.h"
#include "llpanel.h"

class ALPanelRigRim final : public LLPanel
{
public:
    ALPanelRigRim();
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;

private:
    ALRigRimController mRim;
    ALLiveReadoutClock mRefreshClock{ 10.f };
};

#endif
