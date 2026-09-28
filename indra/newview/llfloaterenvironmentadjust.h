/**
 * @file llfloaterenvironmentadjust.h
 * @brief Floaters to create and edit fixed settings for sky and water.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_FLOATERENVIRONMENTADJUST_H
#define LL_FLOATERENVIRONMENTADJUST_H

#include "llfloater.h"
#include "llsettingsbase.h"
#include "llsettingssky.h"
#include "llenvironment.h"

#include <string>
#include <vector>

#include "boost/signals2.hpp"

class LLButton;
class LLLineEditor;
//BD - [BDMerge B13]
class LLComboBox;
class LLFlyoutComboBtnCtrl;

/**
 * Floater container for taking a snapshot of the current environment and making minor adjustments.
 */
class LLFloaterEnvironmentAdjust : public LLFloater
{
    LOG_CLASS(LLFloaterEnvironmentAdjust);

public:
                                LLFloaterEnvironmentAdjust(const LLSD &key);
    virtual                     ~LLFloaterEnvironmentAdjust();


    virtual bool                postBuild() override;
    virtual void                onOpen(const LLSD& key) override;
    virtual void                onClose(bool app_quitting) override;

    virtual void                refresh() override;

private:
    void                        captureCurrentEnvironment();

    void                        onAmbientLightChanged();
    void                        onBlueHorizonChanged();
    void                        onBlueDensityChanged();
    void                        onHazeHorizonChanged();
    void                        onHazeDensityChanged();
    void                        onSceneGammaChanged();

    void                        onCloudColorChanged();
    void                        onCloudCoverageChanged();
    void                        onCloudScaleChanged();
    void                        onSunColorChanged();

    void                        onGlowChanged();
    void                        onStarBrightnessChanged();
    void                        onSunRotationChanged();
    void                        onSunAzimElevChanged();
    void                        onSunScaleChanged();

    void                        onMoonRotationChanged();
    void                        onMoonAzimElevChanged();

    void                        onCloudMapChanged();
    void                        onWaterMapChanged();

    void                        onReflectionProbeAmbianceChanged();
    void                        updateGammaLabel();
    void                        onButtonReset();

    // [EnvIntensity v2] Advanced exposure panel show/hide (AlchemyEnvIntensityAdvanced)
    // and the Sun/Moon link label + Moon slider enable (AlchemyEnvMoonLinked).
    void                        applyEnvIntensityAdvanced();
    void                        updateEnvIntensityMoonLink();

    // [EnvIntensity presets] Quick Preset combo (env_intensity_preset) on the
    // Light Intensity strip. Applying a preset writes the full deterministic
    // set of AlchemyEnv* keys in the ENV_INTENSITY_PRESETS table (llfloaterenvironmentadjust.cpp;
    // never the Advanced-panel toggle, never AlchemyEnvLocalLightIncludeRig,
    // never auto-exposure); mApplyingEnvIntensityPreset suppresses the per-
    // setting listeners below while that write happens. refreshEnvIntensityPresetCombo
    // re-derives the combo's selection from the live settings (exact match ->
    // that preset; otherwise the "Custom" placeholder) and is the handler for
    // every tracked setting's signal, so a manual slider/swatch/Debug Settings
    // edit always falls back to Custom.
    void                        onEnvIntensityPresetSelected();
    void                        applyEnvIntensityPreset(const std::string& key);
    void                        refreshEnvIntensityPresetCombo();

    void                        onEnvironmentUpdated(LLEnvironment::EnvSelection_t env, S32 version);

    // [BDMerge B13] BD - Windlight Stuff (gated by BDMergeEnvLocalPresets;
    //  donor: BD llfloaterenvironmentadjust.cpp/h). Preset name combo,
    //  save-local/save-as flyout, delete/import buttons, and per-axis cloud
    //  scroll lock checkboxes; all controls hidden when the gate is off.
    void                        onButtonApply(LLUICtrl *ctrl, const LLSD &data);
    void                        onSaveAsCommit(const LLSD& notification, const LLSD& response, const LLSettingsBase::ptr_t &settings);
    void                        onInventoryCreated(LLUUID asset_id, LLUUID inventory_id, LLSD results);
    void                        doApplyCreateNewInventory(std::string settings_name, const LLSettingsBase::ptr_t &settings);

    void                        onButtonSave();
    void                        onButtonDelete();
    void                        onButtonImport();
    void                        onSelectPreset();
    void                        loadSkySettingFromFile(const std::vector<std::string>& filenames);

    void                        onCloudScrollXLocked(bool lock);
    void                        onCloudScrollYLocked(bool lock);

    LLComboBox*                 mNameCombo = nullptr;
    LLFlyoutComboBtnCtrl *      mFlyoutControl = nullptr;
    LLUICtrl*                   mCloudScrollLockX = nullptr;
    LLUICtrl*                   mCloudScrollLockY = nullptr;

    LLSettingsSky::ptr_t        mLiveSky;
    LLSettingsWater::ptr_t      mLiveWater;
    LLEnvironment::connection_t mEventConnection;

    // [EnvIntensity] RenderSkyAutoAdjustLegacy change -> updateGammaLabel(),
    // so the Light Intensity classic/legacy note never goes stale. Scoped:
    // disconnects automatically when the floater is destroyed.
    boost::signals2::scoped_connection mEnvIntensityAutoAdjustConn;
    // [EnvIntensity v2] setting listeners (Debug Settings edits show live)
    boost::signals2::scoped_connection mEnvIntensityAdvancedConn;
    boost::signals2::scoped_connection mEnvIntensityMoonLinkedConn;

    // [EnvIntensity presets] one scoped connection per tracked AlchemyEnv* key
    // (see alEnvIntensityPresets()), all routed to refreshEnvIntensityPresetCombo();
    // true while applyEnvIntensityPreset() is writing settings, so that fan-out
    // does not fight the write or flash "Custom" mid-apply.
    std::vector<boost::signals2::scoped_connection> mEnvIntensityPresetConns;
    bool                                mApplyingEnvIntensityPreset = false;
};

#endif // LL_FLOATERFIXEDENVIRONMENT_H
