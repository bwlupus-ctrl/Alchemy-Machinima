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
#include "alkitpresets.h"
#include "alsettingsundohistory.h"
#include "alkitpage.h"
#include "aluirefresh.h"
#include "llbutton.h"
#include "llcontrol.h"
#include "llfloaterenvironmentadjust.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"

#include <memory>

// The injector name must match class="panel_exposure_page" in the generated page XML.
static LLPanelInjector<ALPanelExposurePage> t_panel_exposure_page("panel_exposure_page");

namespace
{
constexpr S32 TONEMAP_AMD_LPM = 7;
constexpr S32 TONEMAP_SLOG3 = 8;

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
};

// The 16 settings a Light Intensity preset owns (alenvintensitypresets.h).
const std::vector<std::string>& intensitySettings()
{
    static const std::vector<std::string> settings = {
        "AlchemyEnvSunEV",            "AlchemyEnvSkyGIEV",           "AlchemyEnvMoonLinked",
        "AlchemyEnvMoonEV",           "AlchemyEnvGIAmbientEV",       "AlchemyEnvGIProbeDiffuseEV",
        "AlchemyEnvGIProbeSpecEV",    "AlchemyEnvLocalLightEV",      "AlchemyEnvSunKelvin",
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
    return LLPanel::postBuild();
}

void ALPanelExposurePage::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateTonemapGating();
        updateHistoryButtons();
    }
    LLPanel::onVisibilityChange(new_visibility);
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
