/** @file alpaneldof.h @brief Shared panel_dof.xml controls. */
#ifndef AL_ALPANELDOF_H
#define AL_ALPANELDOF_H
#include "llpanel.h"
class ALPanelDoF final : public LLPanel
{
public:
    ALPanelDoF();
    bool postBuild() override;
};
#endif
