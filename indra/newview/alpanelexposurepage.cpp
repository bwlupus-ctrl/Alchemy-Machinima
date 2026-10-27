/**
 * @file alpanelexposurepage.cpp
 * @brief Director > Light > Exposure page logic -- see alpanelexposurepage.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelexposurepage.h"

#include "alenvintensitypresets.h"
#include "alfloaterlightbox.h"
#include "alkitbinding.h"
#include "alkitpresets.h"
#include "alsettingsundohistory.h"
#include "alkitpage.h"
#include "alkithistory.h"
#include "alsensorsettings.h"
#include "aluirefresh.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "llfloaterenvironmentadjust.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "pipeline.h"

#include <memory>

// The injector name must match class="panel_exposure_page" in the generated page XML.
static LLPanelInjector<ALPanelExposurePage> t_panel_exposure_page("panel_exposure_page");

namespace
{
constexpr S32 TONEMAP_AMD_LPM = 7;
constexpr S32 TONEMAP_SLOG3 = 8;

bool sensorSeedTextValid(const std::string& text)
{
    return !text.empty() && text.size() <= 10 &&
        text.find_first_not_of("0123456789") == std::string::npos &&
        std::stod(text) <= 4294967295.;
}

class SensorSeedDigits final : public LLTextValidate::ValidatorImpl
{
public:
    bool validate(const std::string& text) override
        { return text.find_first_not_of("0123456789") == std::string::npos; }
    bool validate(const LLWString& text) override
        { return text.find_first_not_of(U"0123456789") == LLWString::npos; }
};

std::string sensorSeedText(const LLSD& value)
{
    // LLSD::Real::asString() uses :g formatting, which rounds large U32 seeds.
    return std::to_string(static_cast<U32>(value.asReal()));
}

class SensorSeedBinding final : public ALKitBinding
{
public:
    LLSD get() const override { return LLSD(sensorSeedText(mSetting.get())); }
    LLSD getDefault() const override { return LLSD(sensorSeedText(mSetting.getDefault())); }
    void set(const LLSD& value) override { mSetting.set(value); }
    void reset() override { mSetting.reset(); }
    boost::signals2::connection connectChanged(std::function<void()> callback) override
        { return mSetting.connectChanged(std::move(callback)); }
    std::string describe() const override { return "CineSensorSeed"; }
private:
    ALKitSettingBinding mSetting{"CineSensorSeed"};
};

struct BuiltinPreset
{
    const char* mKey;     // LLFloaterEnvironmentAdjust::applyEnvIntensityBuiltin key
    const char* mLabel;   // the name Personal Lighting shows
};

const BuiltinPreset BUILTIN_PRESETS[] = {
    { "stock",              "Stock" },
    { "golden_hour",        "Golden Hour" },
    { "blue_hour",          "Blue Hour" },
    { "overcast_soft",      "Overcast Soft" },
    { "noon_punch",         "Noon Punch" },
    { "high_key_studio",    "High-Key Studio" },
    { "low_key_noir",       "Low-Key Noir" },
    { "moonlit_night",      "Moonlit Night" },
    { "neon_night",         "Neon Night" },
    { "candlelit_interior", "Candlelit Interior" },
    { "desert_heat",        "Desert Heat" },
    { "arctic_cold",        "Arctic Cold" },
    { "soft_fill",          "Soft Fill" },
    { "reflections_pop",    "Reflections Pop" },
    { "natural_day", "Natural Day" },
    { "natural_overcast", "Natural Overcast" },
    { "natural_golden_hour", "Natural Golden Hour" },
    { "natural_interior_day", "Natural Interior Day" },
    { "hard_sun", "Hard Sun" },
    { "rainy_day", "Rainy Day" },
    { "natural_moonlight", "Natural Moonlight" },
    { "rainy_night", "Rainy Night" },
    { "night_interior_neon", "Night Interior / Neon" },
};

// The 17 settings a Light Intensity preset owns (alenvintensitypresets.h).
const std::vector<std::string>& intensitySettings()
{
    static const std::vector<std::string> settings = {
        "AlchemyEnvSunEV",            "AlchemyEnvSkyGIEV",           "AlchemyEnvMoonLinked",
        "AlchemyEnvMoonEV",           "AlchemyEnvGIAmbientEV",       "AlchemyEnvGIProbeDiffuseEV",
        "AlchemyEnvGIProbeSpecEV",    "AlchemyEnvLocalLightEV",      "AlchemyEnvSunKelvin",
        "AlchemyEnvLocalLightSize",
        "AlchemyEnvSunTintColor",     "AlchemyEnvSunTintStrength",   "AlchemyEnvMoonTintColor",
        "AlchemyEnvMoonTintStrength", "AlchemyEnvAmbientTintColor",  "AlchemyEnvAmbientTintStrength",
        "AlchemyEnvShadowLiftEV",
    };
    return settings;
}

// The same built-in and saved Light Intensity presets the Personal Lighting combo offers, as a
// Director preset strip.
class EnvIntensityPresetProvider final : public ALKitPresetProvider
{
public:
    EnvIntensityPresetProvider()
    {
        mListConnection = ALEnvIntensityPresets::connectListChanged([this]() { ++mRevision; });
    }
    const std::string& key() const override { return mKey; }
    void list(std::vector<Entry>& out) const override
    {
        for (const BuiltinPreset& preset : BUILTIN_PRESETS)
        {
            out.push_back({ std::string("b:") + preset.mKey, preset.mLabel, true });
        }
        for (const std::string& name : ALEnvIntensityPresets::listNames())
        {
            out.push_back({ "u:" + name, name, false });
        }
    }
    bool apply(const std::string& id) override
    {
        if (id.compare(0, 2, "b:") == 0)
        {
            return LLFloaterEnvironmentAdjust::applyEnvIntensityBuiltin(id.substr(2));
        }
        if (id.compare(0, 2, "u:") == 0)
        {
            ALExposureSettingsHistory::ScopedBatch history_batch("Preset '" + id.substr(2) + "'");
            return ALEnvIntensityPresets::applyNamed(id.substr(2));
        }
        return false;
    }
    bool canSaveUser() const override { return !ALEnvIntensityPresets::isReadOnly(); }
    bool saveUser(const std::string& name) override
    {
        return ALEnvIntensityPresets::saveCurrent(name) == ALEnvIntensityPresets::SAVE_OK;
    }
    bool canDelete(const std::string& id) const override { return id.compare(0, 2, "u:") == 0; }
    bool deleteUser(const std::string& id) override
    {
        return canDelete(id) && ALEnvIntensityPresets::remove(id.substr(2));
    }
    U64 revision() const override { return mRevision; }
    const std::vector<std::string>& trackedSettings() const override { return intensitySettings(); }

private:
    std::string mKey = "env_intensity";
    U64         mRevision = 0;
    boost::signals2::scoped_connection mListConnection;
};

void registerProvider()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;
    ALKitPresetRegistry::instance().add(std::make_unique<EnvIntensityPresetProvider>());
}
} // anonymous namespace

ALPanelExposurePage::ALPanelExposurePage()
:   LLPanel()
{
    // Before the page's children build: the kit page creates its preset strip from the registry.
    registerProvider();
}

bool ALPanelExposurePage::postBuild()
{
    updateEnvOnlyAvailability();
    mEnvOnlyHDRConnection = gSavedSettings.getControl("RenderHDREnabled")->getSignal()->connect(
        [this](LLControlVariable*, const LLSD&, const LLSD&) { updateEnvOnlyAvailability(); });
    mEnvOnlyConnection = gSavedSettings.getControl("AlchemyEnvEnvironmentOnly")->getSignal()->connect(
        [this](LLControlVariable*, const LLSD&, const LLSD&) { updateEnvOnlyAvailability(); });
    mUndo = getChild<LLButton>("exp_undo");
    mRedo = getChild<LLButton>("exp_redo");
    mAMDExposure = findChild<LLUICtrl>("exp_amd_exposure");
    mAMDExposureReset = findChild<LLUICtrl>("exp_amd_exposure_reset");
    mSLogEV = findChild<LLUICtrl>("exp_slog_ev");
    mSLogEVReset = findChild<LLUICtrl>("exp_slog_ev_reset");

    getChild<LLButton>("exp_filters_btn")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { ALFloaterLightBox::showTab("colortone_settings"); });
    mUndo->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { if (ALKitPage* page = findChild<ALKitPage>("exposure_kit")) page->undoOnPage(false); });
    mRedo->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { if (ALKitPage* page = findChild<ALKitPage>("exposure_kit")) page->undoOnPage(true); });
    mHistoryConnection = ALExposureSettingsHistory::connectRevisionChanged(
        [this](U64) { updateHistoryButtons(); });
    // One history step per slider drag or typed value, not one per mouse move.
    ALExposureSettingsHistory::installEditFinalizers(this);

    if (LLControlVariable* tonemap = gSavedSettings.getControl("AlchemyRenderTonemapType"))
    {
        mTonemapConnection = tonemap->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateTonemapGating(); });
    }

    updateTonemapGating();
    updateHistoryButtons();
    mSensorReadout = getChild<LLTextBox>("sensor_readout");
    mSensorStatus = getChild<LLTextBox>("sensor_status");
    mSensorISO = getChild<LLComboBox>("sensor_iso");
    ALKitBindingRegistry::instance().add(this, "sensor_iso",
        []() { return std::make_unique<ALKitSettingBinding>("CineSensorISO"); });
    mSensorISO->setCommitCallback([](LLUICtrl* control, const LLSD&)
        { alKitSetTypedControl(gSavedSettings, "CineSensorISO", control->getValue()); });
    LLLineEditor* seed_editor = getChild<LLLineEditor>("sensor_seed");
    mSensorSeed = seed_editor;
    mSensorSeedRow = seed_editor->getParent();
    // LLSpinCtrl stores F32; a decimal text editor plus the kit's typed writer
    // preserves all U32 seeds, including values above the signed integer range.
    // ALKitSettingBinding still provides reset, history and unsigned read-back.
    ALKitBindingRegistry::instance().add(this, "sensor_seed",
        []() { return std::make_unique<SensorSeedBinding>(); });
    // Allow an empty intermediate edit, but only decimal digits on input.
    static SensorSeedDigits seed_digits;
    seed_editor->setPrevalidate(LLTextValidate::Validator(seed_digits));
    seed_editor->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        if (sensorSeedTextValid(control->getValue().asString()))
            alKitSetTypedControl(gSavedSettings, "CineSensorSeed", control->getValue());
        else
            control->setValue(LLSD(sensorSeedText(LLSD(static_cast<F64>(gSavedSettings.getU32("CineSensorSeed"))))));
    });
    mCustomSensorCard = getChildView("kc_custom_sensor");
    // Native setting bindings own values. Signals invalidate the physical readout,
    // including while hidden; revealing the page always refreshes it immediately.
    for (const char* name : {
        "CineSensorEnabled", "CineSensorISO", "CineSensorPreset", "CineSensorBrightnessMode",
        "CineSensorNoiseAmount", "CineSensorChroma", "CineSensorSeedFixed", "CineSensorSeed",
        "CineSensorCustomBaseISO", "CineSensorCustomSwitchISO", "CineSensorCustomHeadroom",
        "CineSensorCustomLCGFullWell", "CineSensorCustomLCGReadNoise", "CineSensorCustomLCGAnalogBase",
        "CineSensorCustomLCGPedestal", "CineSensorCustomLCGWhite", "CineSensorCustomHCGFullWell",
        "CineSensorCustomHCGReadNoise", "CineSensorCustomHCGAnalogBase", "CineSensorCustomHCGPedestal",
        "CineSensorCustomHCGWhite" })
    {
        const std::string setting(name);
        const bool iso_changed = setting == "CineSensorISO";
        mSensorConnections.emplace_back(gSavedSettings.getControl(name)->getSignal()->connect(
            [this, iso_changed](LLControlVariable*, const LLSD&, const LLSD&)
            {
                if (iso_changed) syncSensorISO();
                updateSensor();
            }));
    }
    syncSensorISO();
    updateSensor();
    showSensorReadout();
    updateSensorStatus();
    return LLPanel::postBuild();
}

void ALPanelExposurePage::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateEnvOnlyAvailability();
        updateTonemapGating();
        updateHistoryButtons();
        syncSensorISO();
        updateSensor();
        showSensorReadout();
        updateSensorStatus();
    }
    LLPanel::onVisibilityChange(new_visibility);
}

void ALPanelExposurePage::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (isWidgetOnScreen(this))
    {
        if (mSensorClock.due())
        {
            if (mSensorDirty) showSensorReadout();
            setValueIfChanged(mSensorISO, mSensorStoredISO);
            setValueIfChanged(mSensorSeed, mSensorStoredSeed);
        }
        if (mSensorStatusClock.due()) updateSensorStatus();
    }
    LLPanel::draw();
}

void ALPanelExposurePage::updateSensor()
{
    if (!mSensorReadout) return;
    updateSensorGating();
    mSensorEnabled = gSavedSettings.getBOOL("CineSensorEnabled");
    mSensorStoredSeed = LLSD(sensorSeedText(LLSD(static_cast<LLSD::Real>(gSavedSettings.getU32("CineSensorSeed")))));
    mSensorDirty = true;
    if (!isInVisibleChain()) return;
    const auto preset = ALSensorSettings::preset();
    const F32 iso = gSavedSettings.getF32("CineSensorISO");
    const auto mode = gSavedSettings.getU32("CineSensorBrightnessMode") == 1 ?
        ALSensorModel::BrightnessMode::Brightens : ALSensorModel::BrightnessMode::Neutral;
    mSensorReadoutText = ALSensorModel::readout(preset, iso, mode);
}

void ALPanelExposurePage::showSensorReadout()
{
    setTextIfChanged(mSensorReadout, mSensorReadoutText);
    mSensorDirty = false;
}

void ALPanelExposurePage::syncSensorISO()
{
    if (!mSensorISO) return;
    // Custom base ISO and externally supplied ISO need not be standard third stops.
    // Preserve them verbatim as a stored-value item so page open never appears blank.
    mSensorStoredISO = gSavedSettings.getControl("CineSensorISO")->get();
    if (!mSensorISO->findItemByValue(mSensorStoredISO.asString()))
        mSensorISO->add(mSensorStoredISO.asString() + " (stored)", mSensorStoredISO);
    setValueIfChanged(mSensorISO, mSensorStoredISO);
}

void ALPanelExposurePage::updateSensorGating()
{
    setEnabledIfChanged(mSensorSeedRow, gSavedSettings.getBOOL("CineSensorSeedFixed"));
    setEnabledIfChanged(mCustomSensorCard, gSavedSettings.getU32("CineSensorPreset") == 3);
}

void ALPanelExposurePage::updateSensorStatus()
{
    if (!mSensorStatus) return;
    const std::string reason = gPipeline.sensorUnavailableReason();
    setTextIfChanged(mSensorStatus, !reason.empty() ? reason :
        mSensorEnabled ? "Sensor ready for the main HDR camera." :
        "Sensor off. Enable Simulate sensor to use the main HDR camera.");
}

// AMD exposure belongs to the AMD LPM tone mapper and S-Log EV to S-Log3 Capture; they are dimmed,
// reset glyph included, for every other tone mapper. Event-driven: the setting signal calls this.
void ALPanelExposurePage::updateTonemapGating()
{
    const S32 tonemap_type = gSavedSettings.getS32("AlchemyRenderTonemapType");
    const bool amd_active = tonemap_type == TONEMAP_AMD_LPM;
    const bool slog_active = tonemap_type == TONEMAP_SLOG3;
    if (mAMDExposure) setEnabledIfChanged(mAMDExposure, amd_active);
    if (mAMDExposureReset) setEnabledIfChanged(mAMDExposureReset, amd_active);
    if (mSLogEV) setEnabledIfChanged(mSLogEV, slog_active);
    if (mSLogEVReset) setEnabledIfChanged(mSLogEVReset, slog_active);
}

void ALPanelExposurePage::updateHistoryButtons()
{
    ALKitPage* page = findChild<ALKitPage>("exposure_kit");
    const bool can_undo = page && page->canUndoOnPage(false);
    const bool can_redo = page && page->canUndoOnPage(true);
    setEnabledIfChanged(mUndo, can_undo);
    setToolTipIfChanged(mUndo, can_undo ? std::string("Undo the last change on this page")
                                       : std::string("Nothing to undo"));
    setEnabledIfChanged(mRedo, can_redo);
    setToolTipIfChanged(mRedo, can_redo ? std::string("Redo the last undone change on this page")
                                       : std::string("Nothing to redo"));
}

void ALPanelExposurePage::updateEnvOnlyAvailability()
{
    const bool hdr = gPipeline.worldHDREnabled();
    const bool enabled = hdr && gSavedSettings.getBOOL("AlchemyEnvEnvironmentOnly");
    if (LLUICtrl* control = findChild<LLUICtrl>("exp_environment_only"))
    {
        setEnabledIfChanged(control, hdr);
    }
    if (LLUICtrl* control = findChild<LLUICtrl>("exp_environment_ev"))
    {
        // Gate row actions too, and disable the native slider so its focused
        // value editor becomes read-only and uses the disabled text colour.
        setEnabledIfChanged(control->getParent(), enabled);
        setEnabledIfChanged(control, enabled);
    }
    for (const char* name : {"exp_environment_ev_label", "exp_environment_ev_reset"})
        if (LLView* control = findChild<LLView>(name)) setEnabledIfChanged(control, enabled);
}
