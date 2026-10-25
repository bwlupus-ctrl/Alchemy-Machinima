/**
 * @file alpanelweathersettings.cpp
 * @brief Shared cinematic weather settings panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"

#include "alpanelweathersettings.h"
#include "alweatheruimodel.h"
#include "llagent.h"
#include "llpresentationtime.h"
#include "llviewerregion.h"
#include "llworld.h"

#include "aluirefresh.h"            // [DirFix] D13 diff-guarded writes

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

const std::vector<std::string>& ALPanelWeatherSettings::trackedSettings()
{
    // Complete persistent weather state. The three Debug-only values are also
    // captured/reset so a named look has no hidden color or seed dependency.
    static const std::vector<std::string> names = {
        "AlchemyWeatherEnabled",
        "AlchemyWeatherControlMode",
        "AlchemyWeatherLocalEnabled",
        "AlchemyWeatherRainRate",
        "AlchemyWeatherPrecipVariation",
        "AlchemyWeatherWindSource",
        "AlchemyWeatherWindDirection",
        "AlchemyWeatherWindSpeed",
        "AlchemyWeatherWindVariation",
        "AlchemyWeatherWindGustiness",
        "AlchemyWeatherWindDirVariation",
        "AlchemyWeatherTransitionSeconds",
        "AlchemyWeatherSeed",
        "AlchemyWeatherStorm",
        "AlchemyWeatherLightningTiming",

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
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [DirFix] D9
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
    // [UIPerf] cached; searched by name only while still null
    if (!mControlsPanel)
    {
        mControlsPanel = findChild<LLPanel>("weather_controls");
    }
    if (!mDeferredNote)
    {
        mDeferredNote = findChild<LLTextBox>("weather_deferred_note");
    }
    if (!mTriggerButton)
    {
        mTriggerButton = findChild<LLButton>("weather_trigger");
    }
    // [DirFix] D13: cached controls (no by-name settings lookup per frame) and
    // diff-guarded widget writes
    static LLCachedControl<bool> weather_enabled(gSavedSettings, "AlchemyWeatherEnabled", false);
    static LLCachedControl<bool> weather_lightning(gSavedSettings, "AlchemyWeatherLightningEnabled", true);
    if (LLPanel* controls = mControlsPanel)
    {
        setEnabledIfChanged(controls, deferred);
    }
    if (LLTextBox* note = mDeferredNote)
    {
        setVisibleIfChanged(note, !deferred);
    }

    // A manual strike also requires the two settings gates. Unlike the shader
    // path, the UI should explain a no-op before the user presses the button.
    if (LLButton* trigger = mTriggerButton)
    {
        setEnabledIfChanged(trigger,
            deferred && weather_enabled && weather_lightning);
    }
}

void ALPanelWeatherSettings::strikeLightningNow()
{
    if (LLPipeline::sRenderDeferred &&
        gSavedSettings.getBOOL("AlchemyWeatherEnabled") &&
        gSavedSettings.getBOOL("AlchemyWeatherLightningEnabled"))
    {
        gSavedSettings.setBOOL("AlchemyWeatherLightningTrigger", true);
    }
}

void ALPanelWeatherSettings::onTriggerStrike()
{
    strikeLightningNow();
}

void ALPanelWeatherSettings::confirmResetAll()
{
    LLNotificationsUtil::add(
        "WeatherConfirmResetAll", LLSD(), LLSD(),
        [](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                resetAllSettingsToDefaults();
            }
            return false;
        });
}

void ALPanelWeatherSettings::onClickResetAll()
{
    confirmResetAll();
}

void ALPanelWeatherSettings::resetAllSettingsToDefaults()
{
    for (const std::string& name : trackedSettings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            control->resetToDefault(true);
        }
    }
    // The one-shot is an action rather than preset state, but Reset All must
    // also cancel any request that has not yet been consumed by the renderer.
    gSavedSettings.setBOOL("AlchemyWeatherLightningTrigger", false);
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
    const std::vector<std::string> names = presetNames();
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

std::vector<std::string> ALPanelWeatherSettings::presetNames()
{
    std::vector<std::string> names;
    LLDirIterator iterator(presetsDir(), "*.xml");
    std::string file;
    while (iterator.next(file))
    {
        names.emplace_back(LLURI::unescape(
            gDirUtilp->getBaseFileName(file, true)));
    }
    std::sort(names.begin(), names.end());
    return names;
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
        saveNamedPreset(name);
        refreshPresetList(name);
    }
    return false;
}

bool ALPanelWeatherSettings::saveNamedPreset(const std::string& name)
{
    if (name.empty())
    {
        return false;
    }
    LLSD values = LLSD::emptyMap();
    for (const std::string& setting : trackedSettings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(setting))
        {
            values[setting] = control->getValue();
        }
    }

    LLSD preset = LLSD::emptyMap();
    preset["version"] = static_cast<S32>(ALWeatherUIModel::PRESET_VERSION);
    preset["settings"] = values;
    const std::string path = presetPath(name);
    llofstream output(path.c_str());
    if (!output.is_open())
    {
        LL_WARNS("Weather") << "Cannot write preset file " << path << LL_ENDL;
        return false;
    }
    const S32 serialized = LLSDSerialize::toPrettyXML(preset, output);
    output.close();
    return serialized > 0 && !output.fail();
}

void ALPanelWeatherSettings::onPresetSelected()
{
    if (mPresetCombo)
    {
        const std::string name = mPresetCombo->getSelectedItemLabel();
        if (!name.empty())
        {
            applyNamedPreset(name);
        }
    }
}

bool ALPanelWeatherSettings::applyNamedPreset(const std::string& name)
{
    if (ALPresetLook::replaying()) return false;
    const std::string path = presetPath(name);
    llifstream input(path.c_str());
    if (!input.is_open())
    {
        LL_WARNS("Weather") << "Cannot open preset file " << path << LL_ENDL;
        return false;
    }
    LLSD preset;
    const S32 parsed = LLSDSerialize::fromXML(preset, input);
    const bool stream_bad = input.bad();
    input.close();
    if (parsed == LLSDParser::PARSE_FAILURE || stream_bad || !preset.isMap() || !preset["settings"].isMap())
    {
        LL_WARNS("Weather") << "Malformed preset file " << path << LL_ENDL;
        return false;
    }

    ALPresetLook::Load load;
    ALPresetLook::reset(trackedSettings());

    // Apply only this panel's allowlisted settings; preset data cannot name an
    // arbitrary viewer control.
    for (const std::string& setting : trackedSettings())
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
    // Unversioned rain looks always use the exact existing renderer path.
    if (!preset.has("version")) gSavedSettings.setU32("AlchemyWeatherControlMode", 0);
    return true;
}

bool ALPanelWeatherSettings::deleteNamedPreset(const std::string& name)
{
    if (name.empty())
    {
        return false;
    }
    const std::string path = presetPath(name);
    if (LLFile::remove(path) != 0)
    {
        LL_WARNS("Weather") << "Cannot delete preset file " << path << LL_ENDL;
        return false;
    }
    return true;
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
    deleteNamedPreset(name);
    refreshPresetList();
    return false;
}

namespace
{
    ALWeatherUIModel sLibraryUI;
    ALWeatherBridge::Settings readLibrarySettings()
    {
        ALWeatherBridge::Settings s;
        static LLCachedControl<bool> master(gSavedSettings, "AlchemyWeatherEnabled", false);
        s.enabled = master();
        static LLCachedControl<U32> c_mode(gSavedSettings, "AlchemyWeatherControlMode", 0U);
        s.mode = c_mode();
        static LLCachedControl<bool> c_local(gSavedSettings, "AlchemyWeatherLocalEnabled", false);
        s.local = c_local();
        static LLCachedControl<F32> c_rainRate(gSavedSettings, "AlchemyWeatherRainRate", 5.f);
        s.rainRate = c_rainRate();
        static LLCachedControl<F32> c_bands(gSavedSettings, "AlchemyWeatherPrecipVariation", 0.35f);
        s.bands = c_bands();
        static LLCachedControl<U32> c_windSource(gSavedSettings, "AlchemyWeatherWindSource", 0U);
        s.windSource = c_windSource();
        static LLCachedControl<F32> c_windDirection(gSavedSettings, "AlchemyWeatherWindDirection", 225.f);
        s.windDirection = c_windDirection();
        static LLCachedControl<F32> c_windSpeed(gSavedSettings, "AlchemyWeatherWindSpeed", 3.f);
        s.windSpeed = c_windSpeed();
        static LLCachedControl<F32> c_windVariation(gSavedSettings, "AlchemyWeatherWindVariation", 0.3f);
        s.windVariation = c_windVariation();
        static LLCachedControl<F32> c_gustiness(gSavedSettings, "AlchemyWeatherWindGustiness", 0.5f);
        s.gustiness = c_gustiness();
        static LLCachedControl<F32> c_directionSwing(gSavedSettings, "AlchemyWeatherWindDirVariation", 0.f);
        s.directionSwing = c_directionSwing();
        static LLCachedControl<F32> c_transition(gSavedSettings, "AlchemyWeatherTransitionSeconds", 20.f);
        s.transition = c_transition();
        static LLCachedControl<U32> c_seed(gSavedSettings, "AlchemyWeatherSeed", 1597463007U);
        s.seed = c_seed();
        static LLCachedControl<F32> c_storm(gSavedSettings, "AlchemyWeatherStorm", 0.f);
        s.storm = c_storm();
        static LLCachedControl<U32> c_lightningTiming(gSavedSettings, "AlchemyWeatherLightningTiming", 0U);
        s.lightningTiming = c_lightningTiming();
        return ALWeatherBridge::sanitize(s);
    }
}

boost::signals2::signal<void()>& ALPanelWeatherSettings::libraryChanged()
{
    static boost::signals2::signal<void()> changed;
    return changed;
}
const std::string& ALPanelWeatherSettings::libraryStatus()
{
    return sLibraryUI.status();
}
void ALPanelWeatherSettings::applyLibraryPreset(U32 preset)
{
    if (preset >= 4) return;
    auto settings = readLibrarySettings();
    ALWeatherUIModel::applyPreset(settings, preset);
    gSavedSettings.setU32("AlchemyWeatherControlMode", settings.mode);
    gSavedSettings.setBOOL("AlchemyWeatherLocalEnabled", settings.local);
    gSavedSettings.setF32("AlchemyWeatherRainRate", settings.rainRate);
    gSavedSettings.setBOOL("AlchemyWeatherRainEnabled", true);
}
void ALPanelWeatherSettings::stopPrecipitation()
{
    if (gSavedSettings.getU32("AlchemyWeatherControlMode") == 0)
    {
        gSavedSettings.setBOOL("AlchemyWeatherRainEnabled", false);
        gSavedSettings.setF32("AlchemyWeatherLightningRate", 0.f);
        return;
    }
    auto settings = readLibrarySettings();
    ALWeatherUIModel::stop(settings);
    gSavedSettings.setF32("AlchemyWeatherRainRate", settings.rainRate);
    gSavedSettings.setF32("AlchemyWeatherStorm", settings.storm);
}
void ALPanelWeatherSettings::updateLibraryWeather()
{
    auto& bridge = ALWeatherBridge::Controller::instance();
    const auto settings = readLibrarySettings();
    ALWeatherModel::Config lightning;
    static LLCachedControl<bool> lightning_Enabled(gSavedSettings, "AlchemyWeatherLightningEnabled", true);
    lightning.mEnabled = lightning_Enabled();
    static LLCachedControl<F32> lightning_Rate(gSavedSettings, "AlchemyWeatherLightningRate", 1.5f);
    lightning.mRatePerMinute = lightning_Rate();
    static LLCachedControl<F32> lightning_FlashDuration(gSavedSettings, "AlchemyWeatherLightningFlashDuration", 0.35f);
    lightning.mFlashDurationSeconds = lightning_FlashDuration();
    static LLCachedControl<F32> lightning_BoltDuration(gSavedSettings, "AlchemyWeatherLightningBoltDuration", 0.18f);
    lightning.mBoltDurationSeconds = lightning_BoltDuration();
    static LLCachedControl<F32> lightning_MinDistance(gSavedSettings, "AlchemyWeatherLightningMinDistance", 45.f);
    lightning.mMinDistanceMeters = lightning_MinDistance();
    static LLCachedControl<F32> lightning_MaxDistance(gSavedSettings, "AlchemyWeatherLightningMaxDistance", 130.f);
    lightning.mMaxDistanceMeters = lightning_MaxDistance();
    static LLCachedControl<F32> lightning_BoltHeight(gSavedSettings, "AlchemyWeatherLightningBoltHeight", 160.f);
    lightning.mBoltHeightMeters = lightning_BoltHeight();
    static LLCachedControl<bool> lightning_QualityEnabled(gSavedSettings, "AlchemyWeatherLightningQualityEnabled", false);
    lightning.mQualityEnabled = lightning_QualityEnabled();
    static LLCachedControl<F32> lightning_AfterglowStrength(gSavedSettings, "AlchemyWeatherLightningAfterglowStrength", 0.35f);
    lightning.mQualityAfterglowStrength = lightning_AfterglowStrength();
    static LLCachedControl<U32> lightning_Seed(gSavedSettings, "AlchemyWeatherLightningSeed", 1597463007U);
    lightning.mSeed = lightning_Seed();
    bool manual = false;
    if (settings.mode == 1 && gSavedSettings.getBOOL("AlchemyWeatherLightningTrigger"))
    {
        manual = true;
        gSavedSettings.setBOOL("AlchemyWeatherLightningTrigger", false);
    }
    const LLVector3 position = gAgent.getPositionAgent();
    const F32 ground = LLWorld::getInstance()->resolveLandHeightAgent(position);
    const ALWeatherModel::Position anchor{position.mV[VX], position.mV[VY], std::isfinite(ground) ? ground : 0.f};
    bridge.update(gFrameCount, LLPresentationTime::currentFrame().presentation_time,
        settings, gWindVec.mV[VX], gWindVec.mV[VY],
        gAgent.getRegion() ? gAgent.getRegion()->getHandle() : 0,
        lightning, anchor, manual);
    if (sLibraryUI.refresh(settings, bridge.state(), bridge.catchingUp(), bridge.clock().seconds())) libraryChanged()();
}
