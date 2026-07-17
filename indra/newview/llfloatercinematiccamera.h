/**
 * @file llfloatercinematiccamera.h
 * @brief Cinematic Camera floater: per-mode auto-hiding parameter panels,
 *        mode/all resets and named global presets stored as LLSD files.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef LL_LLFLOATERCINEMATICCAMERA_H
#define LL_LLFLOATERCINEMATICCAMERA_H

#include "llfloater.h"

#include <string>
#include <vector>

class LLComboBox;

class LLFloaterCinematicCamera final : public LLFloater
{
public:
    LLFloaterCinematicCamera(const LLSD& key);
    ~LLFloaterCinematicCamera() override;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

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

#endif // LL_LLFLOATERCINEMATICCAMERA_H
