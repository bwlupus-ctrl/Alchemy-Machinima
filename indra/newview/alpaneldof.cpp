/** @file alpaneldof.cpp @brief Shared panel_dof.xml controls. */
#include "llviewerprecompiledheaders.h"
#include "alpaneldof.h"
#include "alpanelcinecamparams.h"
#include "llprofiler.h"
#include "lluictrlfactory.h"

static LLPanelInjector<ALPanelDoF> t_panel_dof("panel_dof");

ALPanelDoF::ALPanelDoF()
:   LLPanel()
{
    alRegisterMachinimaResetControl();
}

bool ALPanelDoF::postBuild()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelDoF postBuild");
    return LLPanel::postBuild();
}
