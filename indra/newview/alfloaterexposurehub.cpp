/**
 * @file alfloaterexposurehub.cpp
 * @brief [ExposureHub] One floater that edits the EXISTING exposure / brightness settings.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * See alfloaterexposurehub.h. Nothing here runs in draw(): the floater has no draw override; all
 * work happens at build / open / user-input / setting-change time.
 */

#include "llviewerprecompiledheaders.h"

#include "alfloaterexposurehub.h"

#include "allenslooks.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lltabcontainer.h"
#include "lluictrl.h"
#include "llviewercontrol.h"

#include <cmath>

namespace
{
// Every setting that has a "rst_<setting>" reset button in floater_exposure_hub.xml. The button
// restores the setting's default, same as the reset buttons of the original controls.
const char* const RESET_SETTINGS[] =
{
    // Camera
    "RenderExposure",
    "RenderTonemapMix",
    "AlchemyToneMapAMDExposure",
    "AlchemyToneMapSLogEV",
    // Scene light
    "AlchemyEnvSunEV",
    "AlchemyEnvMoonEV",
    "AlchemyEnvSkyGIEV",
    "AlchemyEnvGIAmbientEV",
    "AlchemyEnvGIProbeDiffuseEV",
    "AlchemyEnvGIProbeSpecEV",
    "AlchemyEnvLocalLightEV",
    "AlchemyEnvShadowLiftEV",
    // Cine Light Rig
    "CineLightRigMasterEV",
    "CineLightRigHeadroomStops",
    // Post
    "RenderBloomStrength",
    "RenderBloomThreshold",
    "RenderGradNDDensity",
    "RenderPolarizerStrength",
    "RenderInfraredStrength",
    "RenderSoftEdgeBlur",
    "CineLightRigNightMaskDarkness",
};

constexpr S32 TONEMAP_AMD_LPM = 7;
constexpr S32 TONEMAP_SLOG3   = 8;
}

ALFloaterExposureHub::ALFloaterExposureHub(const LLSD& key)
:   LLFloater(key)
{
}

bool ALFloaterExposureHub::postBuild()
{
    // [ScrollFit] wraps every tab page in a scroller, fits it to its children, installs
    // focus-reveal and enables wheel adjust on every slider (alscrollfit.h).
    mScrollFit.installForTabs(findChild<LLTabContainer>("exposure_tabs"), this);

    for (const char* setting_name : RESET_SETTINGS)
    {
        const std::string setting(setting_name);
        if (LLUICtrl* button = findChild<LLUICtrl>("rst_" + setting))
        {
            button->setCommitCallback([setting](LLUICtrl*, const LLSD&)
            {
                if (LLControlVariable* control = gSavedSettings.getControl(setting))
                {
                    control->resetToDefault(true);
                }
            });
        }
    }

    // Tonemap-specific rows follow the tonemapper combo (a setting change signal, not draw()).
    if (LLControlVariable* tonemap = gSavedSettings.getControl("AlchemyRenderTonemapType"))
    {
        mTonemapConnection = tonemap->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateTonemapGating(); });
    }
    updateTonemapGating();

    // Night Mask Darkness: percent slider over the 0..1 brightness-floor setting.
    if (LLUICtrl* darkness = findChild<LLUICtrl>("exp_nm_darkness"))
    {
        darkness->setCommitCallback(
            [this](LLUICtrl* control, const LLSD&) { onNightMaskDarknessCommit(control); });
    }
    if (LLControlVariable* floor_control = gSavedSettings.getControl("CineLightRigNightMaskDarkness"))
    {
        mNightMaskConnection = floor_control->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { syncNightMaskDarkness(); });
    }
    syncNightMaskDarkness();

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

    return LLFloater::postBuild();
}

void ALFloaterExposureHub::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    mScrollFit.fit(this, findChild<LLTabContainer>("exposure_tabs"));
    updateTonemapGating();
    syncNightMaskDarkness();
}

void ALFloaterExposureHub::updateTonemapGating()
{
    const S32 tonemap_type = gSavedSettings.getS32("AlchemyRenderTonemapType");
    const bool amd_active = (tonemap_type == TONEMAP_AMD_LPM);
    const bool slog_active = (tonemap_type == TONEMAP_SLOG3);
    // Each reset button follows its slider, so a dimmed row cannot be reset from here either.
    if (LLUICtrl* amd = findChild<LLUICtrl>("exp_amd_exposure"))
    {
        amd->setEnabled(amd_active);
    }
    if (LLUICtrl* amd_reset = findChild<LLUICtrl>("rst_AlchemyToneMapAMDExposure"))
    {
        amd_reset->setEnabled(amd_active);
    }
    if (LLUICtrl* slog = findChild<LLUICtrl>("exp_slog_ev"))
    {
        slog->setEnabled(slog_active);
    }
    if (LLUICtrl* slog_reset = findChild<LLUICtrl>("rst_AlchemyToneMapSLogEV"))
    {
        slog_reset->setEnabled(slog_active);
    }
}

// Same mapping as ALPanelCineLightRig::onNightMaskDarknessCommit: slider percent -> brightness
// floor (the setting keeps its 0..1 meaning: 0 = pitch black, 1 = no darkening).
void ALFloaterExposureHub::onNightMaskDarknessCommit(LLUICtrl* source)
{
    if (!source)
    {
        return;
    }
    // Round first so a typed fractional entry stores exactly the whole percent shown.
    const F32 percent = static_cast<F32>(std::lround(
        llclamp(static_cast<F32>(source->getValue().asReal()), 0.f, 100.f)));
    const F32 floor_value = llclamp(1.f - percent / 100.f, 0.f, 1.f);
    // The setting-change listener refreshes the slider from the new value.
    gSavedSettings.setF32("CineLightRigNightMaskDarkness", floor_value);
    // A same-value write fires no signal; re-sync so a fractional entry snaps to a whole percent.
    syncNightMaskDarkness();
}

void ALFloaterExposureHub::syncNightMaskDarkness()
{
    LLUICtrl* darkness = findChild<LLUICtrl>("exp_nm_darkness");
    if (!darkness)
    {
        return;
    }
    const F32 floor_value = llclamp(
        gSavedSettings.getF32("CineLightRigNightMaskDarkness"), 0.f, 1.f);
    darkness->setValue(static_cast<F32>(std::lround((1.f - floor_value) * 100.f)));
}
