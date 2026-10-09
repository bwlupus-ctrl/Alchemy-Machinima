/** @file alpanelprojectorshafts.h @brief Shared panel_projector_shafts.xml controls. */
#ifndef AL_ALPANELPROJECTORSHAFTS_H
#define AL_ALPANELPROJECTORSHAFTS_H
#include "llpanel.h"
class ALPanelProjectorShafts final : public LLPanel
{
public:
    ALPanelProjectorShafts();
    bool postBuild() override;
};
#endif
