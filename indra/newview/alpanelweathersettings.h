/**
 * @file alpanelweathersettings.h
 * @brief Shared cinematic weather settings, presets, reset, and status panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALPANELWEATHERSETTINGS_H
#define AL_ALPANELWEATHERSETTINGS_H

#include "llpanel.h"

#include <string>
#include <vector>

class LLButton;
class LLComboBox;
class LLTextBox;

class ALPanelWeatherSettings final : public LLPanel
{
public:
    ALPanelWeatherSettings();
    ~ALPanelWeatherSettings() override = default;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

    // Shared with the Director weather kit page (llfloaterdirector.cpp bindWeatherPage).
    static void strikeLightningNow();
    static void initializeLibraryWeather(); // settings notifications, including inactive status
    static void updateLibraryWeather(); // idle, before any view; no draw-time work
    static void applyLibraryPreset(U32 preset);
    static void stopPrecipitation();
    static void applyStartingSurfaces();
    static void clearSurfaces();
    static const std::string& surfaceStatus();
    static const std::string& particleStatus();
    static const std::string& libraryStatus();
    static std::string requirements();
    static void sanitizeLightningDistances();
    static boost::signals2::signal<void()>& libraryChanged();
    static void confirmResetAll();
    static const std::vector<std::string>& trackedSettings();
    static std::vector<std::string> presetNames();
    static bool saveNamedPreset(const std::string& name);
    static bool applyNamedPreset(const std::string& name);
    static bool deleteNamedPreset(const std::string& name);

private:
    static void resetAllSettingsToDefaults();
    static std::string presetsDir();
    static std::string presetPath(const std::string& name);

    void updateDeferredAvailability();
    void updateControlGroup();
    void onTriggerStrike();
    void onClickResetAll();

    void refreshPresetList(const std::string& select_name = std::string());
    void onClickSavePreset();
    bool savePresetCallback(const LLSD& notification, const LLSD& response);
    void onPresetSelected();
    void onClickDeletePreset();
    bool deletePresetCallback(const LLSD& notification, const LLSD& response,
                              const std::string name);

    LLComboBox* mPresetCombo = nullptr;
    LLComboBox* mControlGroupCombo = nullptr;
    // [UIPerf] updateDeferredAvailability() runs every draw; resolve lazily,
    // look up again only while still null.
    LLPanel* mControlsPanel = nullptr;
    LLTextBox* mDeferredNote = nullptr;
    LLButton* mTriggerButton = nullptr;
    std::vector<boost::signals2::scoped_connection> mDistanceConnections;
};

#endif // AL_ALPANELWEATHERSETTINGS_H
