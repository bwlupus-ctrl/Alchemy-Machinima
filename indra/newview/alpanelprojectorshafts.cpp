/** @file alpanelprojectorshafts.cpp @brief Shared panel_projector_shafts.xml controls. */
#include "llviewerprecompiledheaders.h"
#include "alpanelprojectorshafts.h"
#include "alpanelcinecamparams.h"
#include "alprojectorshaftpresets.h"
#include "alprojectorshaftlooks.h"
#include "llviewercontrol.h"
#include "llcombobox.h"
#include "llprofiler.h"
#include "lltextbox.h"
#include "pipeline.h"
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
    mShaftCost = getChild<LLTextBox>("ps_shaft_cost");
    mFlowGrid = getChildView("ps_flow_grid");
    reshape(getRect().getWidth(),getRect().getHeight());
    mPreset = getChild<LLComboBox>("ps_preset");
    mPreset->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        ALProjectorShaftPresets::apply(control->getValue().asString());
    });
    mEngineConnection = gSavedSettings.getControl("BDMergeProjectorVolumetricsEngine")->getSignal()->connect(
        [this](LLControlVariable*, const LLSD&, const LLSD&) { refreshPresets(); });
    refreshPresets();
    return LLPanel::postBuild();
}

void ALPanelProjectorShafts::refreshPresets()
{
    mPreset->clearRows();
    mPreset->add("Custom", LLSD(""));
    if (ALProjectorShaftPresets::classicEngine())
    {
        const auto& labels = ALProjectorShaftPresets::classicLabels();
        for (size_t i = 0; i < labels.size(); ++i) mPreset->add(labels[i], LLSD(std::to_string(i)));
    }
    else
    {
        for (const auto& look : ALProjectorShaftLooks::looks()) mPreset->add(look.label, LLSD(look.id));
    }
    mPreset->selectFirstItem();
}

void ALPanelProjectorShafts::onVisibilityChange(bool visible)
{
    LLPanel::onVisibilityChange(visible);
    if (visible) mCostClock.forceDue();
}

void ALPanelProjectorShafts::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelProjectorShafts draw");
    if (isWidgetOnScreen(mShaftCost) && mCostClock.due())
        setTextIfChanged(mShaftCost,gPipeline.projectorShaftCostReadout());
    LLPanel::draw();
}

void ALPanelProjectorShafts::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width,height,called_from_parent);
    if (!mFlowGrid) mFlowGrid = findChildView("ps_flow_grid");
    if (mFlowGrid)
    {
        LLRect rect = getRect();
        rect.mTop = rect.mBottom + mFlowGrid->getRect().getHeight();
        setRect(rect);
    }
}
