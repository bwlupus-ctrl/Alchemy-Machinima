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

class LLComboBox;

class ALPanelWeatherSettings final : public LLPanel
{
public:
    ALPanelWeatherSettings() = default;
    ~ALPanelWeatherSettings() override = default;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    static const std::vector<std::string>& settings();
    static std::string presetsDir();
    static std::string presetPath(const std::string& name);

    void updateDeferredAvailability();
    void onTriggerStrike();
    void onClickResetAll();
    bool resetAllCallback(const LLSD& notification, const LLSD& response);

    void refreshPresetList(const std::string& select_name = std::string());
    void onClickSavePreset();
    bool savePresetCallback(const LLSD& notification, const LLSD& response);
    void writePreset(const std::string& name);
    void onPresetSelected();
    void applyPreset(const std::string& name);
    void onClickDeletePreset();
    bool deletePresetCallback(const LLSD& notification, const LLSD& response,
                              const std::string name);

    LLComboBox* mPresetCombo = nullptr;
};

#endif // AL_ALPANELWEATHERSETTINGS_H
