/** @file alpanelprojectorshafts.cpp @brief Shared panel_projector_shafts.xml controls. */
#include "llviewerprecompiledheaders.h"
#include "alpanelprojectorshafts.h"
#include "alpanelcinecamparams.h"
#include "alprojectorshaftpresets.h"
#include "llcombobox.h"
#include "llprofiler.h"
#include "lluictrlfactory.h"

static LLPanelInjector<ALPanelProjectorShafts> t_panel_projector_shafts("panel_projector_shafts");

ALPanelProjectorShafts::ALPanelProjectorShafts()
:   LLPanel()
{
    alRegisterMachinimaResetControl();
}

bool ALPanelProjectorShafts::postBuild()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelProjectorShafts postBuild");
    getChild<LLComboBox>("ps_preset")->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        ALProjectorShaftPresets::apply(control->getValue().asInteger());
    });
    return LLPanel::postBuild();
}
