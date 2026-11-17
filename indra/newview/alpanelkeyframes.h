#ifndef AL_PANEL_KEYFRAMES_H
#define AL_PANEL_KEYFRAMES_H
#include "llpanel.h"
class ALPanelKeyframes : public LLPanel
{
public:
    ALPanelKeyframes();
    ~ALPanelKeyframes() override;
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;
};
#endif
