/**
 * @file llfloaterenvironmentsettings.h
 * @brief LLFloaterEnvironmentSettings class definition
 *
 * [BDMerge B13] Ported from Black Dragon (donor:
 * I:\black-dragon\indra\newview\llfloaterenvironmentsettings.h, NiranV Dean).
 * BD's Windlight-style environment-settings window: region toggle plus
 * sky/water/day-cycle preset combos fed from disk and inventory.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
 * Copyright (C) 2018, NiranV Dean (Black Dragon Viewer)
 * Copyright (C) 2026, bwlupus-ctrl (machinima fork, BD merge campaign item B13)
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

#ifndef LL_LLFLOATERENVIRONMENTSETTINGS_H
#define LL_LLFLOATERENVIRONMENTSETTINGS_H

#include "llfloater.h"
#include "llsettingssky.h"
#include "llsettingswater.h"
#include "llenvironment.h"

class LLComboBox;
class LLRadioGroup;
//BD
class LLCheckBoxCtrl;

class LLFloaterEnvironmentSettings : public LLFloater
{
    LOG_CLASS(LLFloaterEnvironmentSettings);

public:
    LLFloaterEnvironmentSettings(const LLSD &key);
    /*virtual*/ bool    postBuild() override;
    /*virtual*/ void    onOpen(const LLSD& key) override;

private:
    void onSwitchRegionSettings();
    void onChangeToRegion();

    void onSelectWaterPreset();
    void onSelectSkyPreset();
    void onSelectDayCyclePreset();

    void onBtnCancel();

    void refresh(); /// update controls with user prefs

    void populateWaterPresetsList();
    void populateSkyPresetsList();
    void populateDayCyclePresetsList();

    //BD
    LLButton*       mRegionSettingsButton;
    LLCheckBoxCtrl* mDayCycleSettingsCheck;

    LLComboBox*     mWaterPresetCombo;
    LLComboBox*     mSkyPresetCombo;
    LLComboBox*     mDayCyclePresetCombo;

    LLSettingsSky::ptr_t        mLiveSky;
    LLSettingsWater::ptr_t      mLiveWater;
    LLSettingsDay::ptr_t        mLiveDay;
};

#endif // LL_LLFLOATERENVIRONMENTSETTINGS_H
