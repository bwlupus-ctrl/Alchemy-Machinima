/**
 * @file alpanelweathersettings.cpp
 * @brief Shared cinematic weather settings panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelweathersettings.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfile.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lltextbox.h"
#include "lluri.h"
#include "llviewercontrol.h"
#include "pipeline.h"

#include <algorithm>

static LLPanelInjector<ALPanelWeatherSettings>
    t_panel_weather_settings("panel_weather_settings");

namespace
{
constexpr char PRESET_SUBDIR[] = "weather_presets";

void registerWeatherResetControl()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;
    LLUICtrl::CommitCallbackRegistry::defaultRegistrar().add(
        "Weather.ResetControl",
        [](LLUICtrl*, const LLSD& param)
        {
            if (LLControlVariable* control =
                    gSavedSettings.getControl(param.asString()))
            {
                control->resetToDefault(true);
            }
        });
}
}

ALPanelWeatherSettings::ALPanelWeatherSettings()
{
    // XUI commit callbacks are resolved while the panel's children are built.
    registerWeatherResetControl();
}

const std::vector<std::string>& ALPanelWeatherSettings::settings()
{
    // Complete persistent weather state. The three Debug-only values are also
    // captured/reset so a named look has no hidden color or seed dependency.
    static const std::vector<std::string> names = {
        "AlchemyWeatherEnabled",
        "AlchemyWeatherRainEnabled",
        "AlchemyWeatherRainIntensity",
        "AlchemyWeatherRainDensity",
        "AlchemyWeatherRainFallSpeed",
        "AlchemyWeatherRainMaxDistance",
        "AlchemyWeatherRainSamples",
        "AlchemyWeatherRainResolutionDivisor",
        "AlchemyWeatherRainLayers",
        "AlchemyWeatherRainVirtualShutter",
        "AlchemyWeatherRainNearEmphasis",
        "AlchemyWeatherRainGustStrength",
        "AlchemyWeatherShelterFade",
        "AlchemyWeatherShelterHeight",
        "AlchemyWeatherRainOcclusion",
        "AlchemyWeatherRainOcclusionResolution",
        "AlchemyWeatherRainOcclusionExtent",
        "AlchemyWeatherRainOcclusionBias",
        "AlchemyWeatherRainOcclusionSoftness",
        "AlchemyWeatherWindScale",
        "AlchemyWeatherRainColor",
        "AlchemyWeatherSplashEnabled",
        "AlchemyWeatherSplashDensity",
        "AlchemyWeatherSplashRingSize",
        "AlchemyWeatherSplashLifetime",
        "AlchemyWeatherSplashUpThreshold",
        "AlchemyWeatherSplashMaxDistance",
        "AlchemyWeatherWetnessEnabled",
        "AlchemyWeatherWetnessStrength",
        "AlchemyWeatherMistEnabled",
        "AlchemyWeatherMistStrength",
        "AlchemyWeatherMistHeight",
        "AlchemyWeatherLensDropsEnabled",
        "AlchemyWeatherLensDropsStrength",
        "AlchemyWeatherEEPCoupling",
        "AlchemyWeatherLightningEnabled",
        "AlchemyWeatherLightningRate",
        "AlchemyWeatherLightningFlashDuration",
        "AlchemyWeatherLightningBoltDuration",
        "AlchemyWeatherLightningMinDistance",
        "AlchemyWeatherLightningMaxDistance",
        "AlchemyWeatherLightningBoltHeight",
        "AlchemyWeatherLightningBoltWidth",
        "AlchemyWeatherLightningBrightness",
        "AlchemyWeatherLightningAmbient",
        "AlchemyWeatherLightningColor",
        "AlchemyWeatherLightningSeed",
        "AlchemyWeatherLightningQualityEnabled",
        "AlchemyWeatherLightningQualityTier",
        "AlchemyWeatherLightningSheetEnabled",
        "AlchemyWeatherLightningSheetStrength",
        "AlchemyWeatherLightningCoronaStrength",
        "AlchemyWeatherLightningWetGlintEnabled",
        "AlchemyWeatherLightningWetGlintStrength",
        "AlchemyWeatherLightningDistanceGrading",
        "AlchemyWeatherLightningAfterglowStrength",
        "AlchemyWeatherLightningEnergyCeiling",
    };
    return names;
}

bool ALPanelWeatherSettings::postBuild()
{
    mControlGroupCombo = getChild<LLComboBox>("weather_control_group");
    mControlGroupCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { updateControlGroup(); });
    getChild<LLButton>("weather_trigger")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onTriggerStrike(); });
    getChild<LLButton>("weather_reset_all")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickResetAll(); });
    getChild<LLButton>("weather_preset_save")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickSavePreset(); });
    getChild<LLButton>("weather_preset_delete")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickDeletePreset(); });

    mPresetCombo = getChild<LLComboBox>("weather_preset_combo");
    mPresetCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onPresetSelected(); });
    updateControlGroup();
    refreshPresetList();
    updateDeferredAvailability();
    return true;
}

void ALPanelWeatherSettings::draw()
{
    updateDeferredAvailability();
    LLPanel::draw();
}

void ALPanelWeatherSettings::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        refreshPresetList(
            mPresetCombo ? mPresetCombo->getSelectedItemLabel() : std::string());
        updateDeferredAvailability();
    }
    LLPanel::onVisibilityChange(new_visibility);
}

void ALPanelWeatherSettings::updateControlGroup()
{
    const std::string selected = mControlGroupCombo
        ? mControlGroupCombo->getSelectedValue().asString() : "rain";
    if (LLPanel* rain = findChild<LLPanel>("weather_rain_group"))
    {
        rain->setVisible(selected == "rain");
    }
    if (LLPanel* surface = findChild<LLPanel>("weather_surface_group"))
    {
        surface->setVisible(selected == "surface");
    }
    if (LLPanel* occlusion = findChild<LLPanel>("weather_occlusion_group"))
    {
        occlusion->setVisible(selected == "occlusion");
    }
    if (LLPanel* lightning = findChild<LLPanel>("weather_lightning_group"))
    {
        lightning->setVisible(selected == "lightning");
    }
}

void ALPanelWeatherSettings::updateDeferredAvailability()
{
    const bool deferred = LLPipeline::sRenderDeferred;
    if (LLPanel* controls = findChild<LLPanel>("weather_controls"))
    {
        controls->setEnabled(deferred);
    }
    if (LLTextBox* note = findChild<LLTextBox>("weather_deferred_note"))
    {
        note->setVisible(!deferred);
    }

    // A manual strike also requires the two settings gates. Unlike the shader
    // path, the UI should explain a no-op before the user presses the button.
    if (LLButton* trigger = findChild<LLButton>("weather_trigger"))
    {
        trigger->setEnabled(
            deferred &&
            gSavedSettings.getBOOL("AlchemyWeatherEnabled") &&
            gSavedSettings.getBOOL("AlchemyWeatherLightningEnabled"));
    }
}

void ALPanelWeatherSettings::onTriggerStrike()
{
    if (LLPipeline::sRenderDeferred &&
        gSavedSettings.getBOOL("AlchemyWeatherEnabled") &&
        gSavedSettings.getBOOL("AlchemyWeatherLightningEnabled"))
    {
        gSavedSettings.setBOOL("AlchemyWeatherLightningTrigger", true);
    }
}

void ALPanelWeatherSettings::onClickResetAll()
{
    LLHandle<ALPanelWeatherSettings> handle =
        getDerivedHandle<ALPanelWeatherSettings>();
    LLNotificationsUtil::add(
        "WeatherConfirmResetAll", LLSD(), LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelWeatherSettings* self = handle.get())
            {
                self->resetAllCallback(notification, response);
            }
        });
}

bool ALPanelWeatherSettings::resetAllCallback(
    const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    for (const std::string& name : settings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            control->resetToDefault(true);
        }
    }
    // The one-shot is an action rather than preset state, but Reset All must
    // also cancel any request that has not yet been consumed by the renderer.
    gSavedSettings.setBOOL("AlchemyWeatherLightningTrigger", false);
    return false;
}

std::string ALPanelWeatherSettings::presetsDir()
{
    const std::string dir = gDirUtilp->getExpandedFilename(
        LL_PATH_USER_SETTINGS, PRESET_SUBDIR);
    if (!gDirUtilp->fileExists(dir))
    {
        LLFile::mkdir(dir);
    }
    return dir;
}

std::string ALPanelWeatherSettings::presetPath(const std::string& name)
{
    return gDirUtilp->add(presetsDir(), LLURI::escape(name) + ".xml");
}

void ALPanelWeatherSettings::refreshPresetList(
    const std::string& select_name)
{
    if (!mPresetCombo)
    {
        return;
    }
    mPresetCombo->clearRows();
    std::vector<std::string> names;
    LLDirIterator iterator(presetsDir(), "*.xml");
    std::string file;
    while (iterator.next(file))
    {
        names.emplace_back(LLURI::unescape(
            gDirUtilp->getBaseFileName(file, true)));
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        mPresetCombo->add(name);
    }
    if (!select_name.empty() &&
        mPresetCombo->setSelectedByValue(select_name, true))
    {
        return;
    }
    mPresetCombo->setLabel(getString("no_preset_label"));
}

void ALPanelWeatherSettings::onClickSavePreset()
{
    LLSD args;
    args["DESC"] =
        mPresetCombo && !mPresetCombo->getSelectedItemLabel().empty()
            ? mPresetCombo->getSelectedItemLabel() : LLStringUtil::null;
    LLHandle<ALPanelWeatherSettings> handle =
        getDerivedHandle<ALPanelWeatherSettings>();
    LLNotificationsUtil::add(
        "WeatherSavePreset", args, LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelWeatherSettings* self = handle.get())
            {
                self->savePresetCallback(notification, response);
            }
        });
}

bool ALPanelWeatherSettings::savePresetCallback(
    const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    std::string name = response["message"].asString();
    LLStringUtil::trim(name);
    if (!name.empty())
    {
        writePreset(name);
        refreshPresetList(name);
    }
    return false;
}

void ALPanelWeatherSettings::writePreset(const std::string& name)
{
    LLSD values = LLSD::emptyMap();
    for (const std::string& setting : settings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(setting))
        {
            values[setting] = control->getValue();
        }
    }

    LLSD preset = LLSD::emptyMap();
    preset["settings"] = values;
    const std::string path = presetPath(name);
    llofstream output(path.c_str());
    if (!output.is_open())
    {
        LL_WARNS("Weather") << "Cannot write preset file " << path << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(preset, output);
    output.close();
}

void ALPanelWeatherSettings::onPresetSelected()
{
    if (mPresetCombo)
    {
        const std::string name = mPresetCombo->getSelectedItemLabel();
        if (!name.empty())
        {
            applyPreset(name);
        }
    }
}

void ALPanelWeatherSettings::applyPreset(const std::string& name)
{
    const std::string path = presetPath(name);
    llifstream input(path.c_str());
    if (!input.is_open())
    {
        LL_WARNS("Weather") << "Cannot open preset file " << path << LL_ENDL;
        return;
    }
    LLSD preset;
    LLSDSerialize::fromXML(preset, input);
    input.close();
    if (!preset.isMap() || !preset["settings"].isMap())
    {
        LL_WARNS("Weather") << "Malformed preset file " << path << LL_ENDL;
        return;
    }

    // Apply only this panel's allowlisted settings; preset data cannot name an
    // arbitrary viewer control.
    for (const std::string& setting : settings())
    {
        if (preset["settings"].has(setting))
        {
            if (LLControlVariable* control =
                    gSavedSettings.getControl(setting))
            {
                control->setValue(preset["settings"][setting]);
            }
        }
    }
}

void ALPanelWeatherSettings::onClickDeletePreset()
{
    if (!mPresetCombo)
    {
        return;
    }
    const std::string name = mPresetCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLSD args;
    args["NAME"] = name;
    LLHandle<ALPanelWeatherSettings> handle =
        getDerivedHandle<ALPanelWeatherSettings>();
    LLNotificationsUtil::add(
        "WeatherConfirmDeletePreset", args, LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelWeatherSettings* self = handle.get())
            {
                self->deletePresetCallback(notification, response, name);
            }
        });
}

bool ALPanelWeatherSettings::deletePresetCallback(
    const LLSD& notification, const LLSD& response, const std::string name)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    const std::string path = presetPath(name);
    if (LLFile::remove(path) != 0)
    {
        LL_WARNS("Weather") << "Cannot delete preset file " << path << LL_ENDL;
    }
    refreshPresetList();
    return false;
}
