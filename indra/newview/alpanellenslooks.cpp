/**
 * @file alpanellenslooks.cpp
 * @brief Shared Lens Looks controls from panel_lens_looks.xml.
 */
#include "llviewerprecompiledheaders.h"

#include "alpanellenslooks.h"

#include "allenslooks.h"
#include "alpanelcinecamparams.h"
#include "aluirefresh.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "llnotificationsutil.h"
#include "llprofiler.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"

static LLPanelInjector<ALPanelLensLooks> t_panel_lens_looks("panel_lens_looks");

ALPanelLensLooks::ALPanelLensLooks()
:   LLPanel()
{
    alRegisterMachinimaResetControl();
}

ALPanelLensLooks::~ALPanelLensLooks()
{
    mInfraredEnabledConnection.disconnect();
    mAvatarTagConnection.disconnect();
    mSoftEdgeEnabledConnection.disconnect();
    mSoftEdgeBlurTypeConnection.disconnect();
    mDoFConnection.disconnect();
}

bool ALPanelLensLooks::postBuild()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelLensLooks postBuild");
    ALLensLooks::bindFilterPresets(this);

    mInfraredHint = getChild<LLTextBox>("ir_tag_hint");
    mInfraredEnableTag = getChild<LLButton>("ir_enable_tag");
    mSoftEdgeDoFHint = getChild<LLTextBox>("softedge_dof_hint");

    getChild<LLComboBox>("ir_preset_action")->setCommitCallback(
        [](LLUICtrl* control, const LLSD&)
        {
            ALLensLooks::applyInfraredPreset(control->getValue().asInteger());
            control->setValue(LLSD("0"));
        });
    getChild<LLComboBox>("softedge_preset_action")->setCommitCallback(
        [](LLUICtrl* control, const LLSD&)
        {
            ALLensLooks::applySoftEdgePreset(control->getValue().asInteger());
            control->setValue(LLSD("0"));
        });
    mInfraredEnableTag->setCommitCallback([](LLUICtrl*, const LLSD&)
    {
        LLNotificationsUtil::add("EnableGBufferAvatarTagConfirm", LLSD(), LLSD(),
            [](const LLSD& notification, const LLSD& response)
            {
                if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                {
                    gSavedSettings.setBOOL("RenderGBufferAvatarTag", true);
                }
                return false;
            });
    });

    auto hint_signal = [this](LLControlVariable*, const LLSD&, const LLSD&) { updateHints(); };
    mInfraredEnabledConnection =
        gSavedSettings.getControl("RenderInfraredEnabled")->getSignal()->connect(hint_signal);
    mAvatarTagConnection =
        gSavedSettings.getControl("RenderGBufferAvatarTag")->getSignal()->connect(hint_signal);
    mSoftEdgeEnabledConnection =
        gSavedSettings.getControl("RenderSoftEdgeEnabled")->getSignal()->connect(hint_signal);
    mSoftEdgeBlurTypeConnection =
        gSavedSettings.getControl("RenderSoftEdgeBlurType")->getSignal()->connect(hint_signal);
    mDoFConnection =
        gSavedSettings.getControl("RenderDepthOfField")->getSignal()->connect(hint_signal);
    updateHints();
    return LLPanel::postBuild();
}

void ALPanelLensLooks::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateHints();
    }
    LLPanel::onVisibilityChange(new_visibility);
}

void ALPanelLensLooks::updateHints()
{
    const bool ir = gSavedSettings.getBOOL("RenderInfraredEnabled");
    const bool tag_wanted = gSavedSettings.getBOOL("RenderGBufferAvatarTag");
    const bool tag = LLViewerShaderMgr::gbufferAvatarTagActive();
    setVisibleIfChanged(mInfraredHint, ir && (!tag || gOnLensNoIR));
    setVisibleIfChanged(mInfraredEnableTag, !tag_wanted);
    setTextIfChanged(mInfraredHint, gOnLensNoIR
        ? std::string("Infrared unavailable on this driver")
        : std::string("Skin needs avatar tag"));
    const bool defocus = gSavedSettings.getBOOL("RenderSoftEdgeEnabled") &&
        gSavedSettings.getU32("RenderSoftEdgeBlurType") == 1 &&
        !gSavedSettings.getBOOL("RenderDepthOfField");
    setVisibleIfChanged(mSoftEdgeDoFHint, defocus);
}
