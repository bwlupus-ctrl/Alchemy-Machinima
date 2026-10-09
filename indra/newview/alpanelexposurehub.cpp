/**
 * @file alpanelexposurehub.cpp
 * @brief Shared Exposure controls from panel_exposure_hub.xml.
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelexposurehub.h"

#include "alfloaterlightbox.h"
#include "allenslooks.h"
#include "alpanelcinecamparams.h"
#include "alsettingsundohistory.h"
#include "aluirefresh.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "llprofiler.h"
#include "lltabcontainer.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"

#include <cmath>

static LLPanelInjector<ALPanelExposureHub> t_panel_exposure_hub("panel_exposure_hub");

namespace
{
const char* const RESET_SETTINGS[] =
{
    "RenderExposure",
    "RenderTonemapMix",
    "AlchemyToneMapAMDExposure",
    "AlchemyToneMapSLogEV",
    "AlchemyEnvSunEV",
    "AlchemyEnvMoonEV",
    "AlchemyEnvSkyGIEV",
    "AlchemyEnvGIAmbientEV",
    "AlchemyEnvGIProbeDiffuseEV",
    "AlchemyEnvGIProbeSpecEV",
    "AlchemyEnvLocalLightEV",
    "AlchemyEnvShadowLiftEV",
    "CineLightRigMasterEV",
    "CineLightRigHeadroomStops",
    "RenderBloomStrength",
    "RenderBloomThreshold",
    "RenderGradNDDensity",
    "RenderPolarizerStrength",
    "RenderInfraredStrength",
    "RenderSoftEdgeBlur",
    "CineLightRigNightMaskDarkness",
};

constexpr S32 TONEMAP_AMD_LPM = 7;
constexpr S32 TONEMAP_SLOG3 = 8;
}

ALPanelExposureHub::ALPanelExposureHub()
:   LLPanel()
{
    alRegisterMachinimaResetControl();
}

bool ALPanelExposureHub::postBuild()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelExposureHub postBuild");

    mTabs = getChild<LLTabContainer>("exposure_tabs");
    mUndo = getChild<LLButton>("exp_undo");
    mRedo = getChild<LLButton>("exp_redo");
    mAMDExposure = findChild<LLUICtrl>("exp_amd_exposure");
    mAMDExposureReset = findChild<LLUICtrl>("rst_AlchemyToneMapAMDExposure");
    mSLogEV = findChild<LLUICtrl>("exp_slog_ev");
    mSLogEVReset = findChild<LLUICtrl>("rst_AlchemyToneMapSLogEV");
    mNightMaskDarkness = findChild<LLUICtrl>("exp_nm_darkness");

    mScrollFit.installForTabs(mTabs, this);

    for (const char* setting_name : RESET_SETTINGS)
    {
        const std::string setting(setting_name);
        if (LLUICtrl* button = findChild<LLUICtrl>("rst_" + setting))
        {
            button->setCommitCallback([setting](LLUICtrl*, const LLSD&)
            {
                if (LLControlVariable* control = gSavedSettings.getControl(setting))
                {
                    if (const auto* tracked = ALExposureSettingsHistory::findTrackedSetting(setting))
                    {
                        ALExposureSettingsHistory::ScopedBatch batch(
                            "Reset " + std::string(tracked->mLabel));
                        control->resetToDefault(true);
                    }
                    else
                    {
                        control->resetToDefault(true);
                    }
                }
            });
        }
    }

    if (LLControlVariable* tonemap = gSavedSettings.getControl("AlchemyRenderTonemapType"))
    {
        mTonemapConnection = tonemap->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateTonemapGating(); });
    }

    if (mNightMaskDarkness)
    {
        mNightMaskDarkness->setCommitCallback(
            [this](LLUICtrl* control, const LLSD&) { onNightMaskDarknessCommit(control); });
    }
    if (LLControlVariable* floor_control = gSavedSettings.getControl("CineLightRigNightMaskDarkness"))
    {
        mNightMaskConnection = floor_control->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { syncNightMaskDarkness(); });
    }

    getChild<LLComboBox>("exp_ir_preset")->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        ALLensLooks::applyInfraredPreset(control->getValue().asInteger());
        control->setValue(LLSD("0"));
    });
    getChild<LLComboBox>("exp_softedge_preset")->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        ALLensLooks::applySoftEdgePreset(control->getValue().asInteger());
        control->setValue(LLSD("0"));
    });
    getChild<LLButton>("exp_filters_btn")->setCommitCallback([](LLUICtrl*, const LLSD&)
    {
        ALFloaterLightBox::showTab("colortone_settings");
    });

    mUndo->setCommitCallback([](LLUICtrl*, const LLSD&) { ALExposureSettingsHistory::undo(); });
    mRedo->setCommitCallback([](LLUICtrl*, const LLSD&) { ALExposureSettingsHistory::redo(); });
    mHistoryConnection = ALExposureSettingsHistory::connectRevisionChanged(
        [this](U64) { updateHistoryButtons(); });

    ALExposureSettingsHistory::installEditFinalizers(findChild<LLPanel>("exp_tab_camera"));
    ALExposureSettingsHistory::installEditFinalizers(findChild<LLPanel>("exp_tab_light"));

    updateTonemapGating();
    syncNightMaskDarkness();
    updateHistoryButtons();
    return LLPanel::postBuild();
}

void ALPanelExposureHub::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        mScrollFit.fit(this, mTabs);
        updateTonemapGating();
        syncNightMaskDarkness();
        updateHistoryButtons();
    }
    LLPanel::onVisibilityChange(new_visibility);
}

void ALPanelExposureHub::updateTonemapGating()
{
    const S32 tonemap_type = gSavedSettings.getS32("AlchemyRenderTonemapType");
    const bool amd_active = tonemap_type == TONEMAP_AMD_LPM;
    const bool slog_active = tonemap_type == TONEMAP_SLOG3;
    if (mAMDExposure) mAMDExposure->setEnabled(amd_active);
    if (mAMDExposureReset) mAMDExposureReset->setEnabled(amd_active);
    if (mSLogEV) mSLogEV->setEnabled(slog_active);
    if (mSLogEVReset) mSLogEVReset->setEnabled(slog_active);
}

void ALPanelExposureHub::onNightMaskDarknessCommit(LLUICtrl* source)
{
    if (!source)
    {
        return;
    }
    const F32 percent = static_cast<F32>(std::lround(
        llclamp(static_cast<F32>(source->getValue().asReal()), 0.f, 100.f)));
    gSavedSettings.setF32("CineLightRigNightMaskDarkness",
                         llclamp(1.f - percent / 100.f, 0.f, 1.f));
    syncNightMaskDarkness();
}

void ALPanelExposureHub::syncNightMaskDarkness()
{
    if (!mNightMaskDarkness)
    {
        return;
    }
    const F32 floor_value = llclamp(
        gSavedSettings.getF32("CineLightRigNightMaskDarkness"), 0.f, 1.f);
    setValueIfChanged(mNightMaskDarkness,
        LLSD(static_cast<F32>(std::lround((1.f - floor_value) * 100.f))), true);
}

void ALPanelExposureHub::updateHistoryButtons()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ExposureHub updateHistoryButtons");
    const bool can_undo = ALExposureSettingsHistory::canUndo();
    const bool can_redo = ALExposureSettingsHistory::canRedo();
    setEnabledIfChanged(mUndo, can_undo);
    setToolTipIfChanged(mUndo, can_undo ? "Undo: " + ALExposureSettingsHistory::undoLabel()
                                       : "Nothing to undo");
    setEnabledIfChanged(mRedo, can_redo);
    setToolTipIfChanged(mRedo, can_redo ? "Redo: " + ALExposureSettingsHistory::redoLabel()
                                       : "Nothing to redo");
}
