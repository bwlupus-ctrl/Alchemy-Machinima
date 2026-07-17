/**
 * @file alpanelcinecamparams.h
 * @brief Cinematic Camera parameter panel: shared header, per-mode
 *        auto-hiding parameter panels, mode/all resets and named global
 *        presets stored as LLSD files.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Extracted from LLFloaterCinematicCamera so the standalone Cinematic
 * Camera floater and the Director Console can embed the SAME panel
 * (panel_cinecam_params.xml) instead of duplicating 22 mode panels of
 * XML. Every control is settings-backed (control_name), so multiple
 * live instances stay in sync through gSavedSettings; the panel holds
 * no floater-singleton state.
 */

#ifndef AL_ALPANELCINECAMPARAMS_H
#define AL_ALPANELCINECAMPARAMS_H

#include "llpanel.h"

#include <string>
#include <vector>

class LLComboBox;

class ALPanelCineCamParams final : public LLPanel
{
public:
    ALPanelCineCamParams() = default;
    ~ALPanelCineCamParams() override;

    bool postBuild() override;
    // each embedding floater's onOpen lands here: re-sync the mode panel
    // and pick up presets saved from another instance
    void onVisibilityChange(bool new_visibility) override;

private:
    // One entry per CinematicCamMode value: the params panel that exposes it
    // and every CinematicCam* setting that mode's pattern function reads
    // (ground truth: the LLCachedControl declarations in llcinematiccamera.cpp).
    struct ModeEntry
    {
        S32                      mMode;
        const char*              mPanel;
        std::vector<std::string> mSettings;
    };
    static const std::vector<ModeEntry>&    modeTable();
    // Settings read for every mode (enable/mode/targeting/operator/smoothing/dutch).
    static const std::vector<std::string>&  sharedSettings();

    // per-mode panel visibility
    void updateModePanel();

    // resets
    void onClickResetMode();
    void onClickResetAll();
    bool resetAllCallback(const LLSD& notification, const LLSD& response);

    // named global presets (hand-rolled LLSD .xml files)
    static std::string presetsDir();                    // created on demand
    static std::string presetPath(const std::string& name);
    void refreshPresetList(const std::string& select_name = std::string());
    void onClickSavePreset();
    bool savePresetCallback(const LLSD& notification, const LLSD& response);
    void onPresetSelected();
    void onClickDeletePreset();
    bool deletePresetCallback(const LLSD& notification, const LLSD& response,
                              const std::string name);
    void writePreset(const std::string& name);
    void applyPreset(const std::string& name);

    LLComboBox* mPresetCombo = nullptr;
    boost::signals2::connection mModeConnection;
};

#endif // AL_ALPANELCINECAMPARAMS_H
