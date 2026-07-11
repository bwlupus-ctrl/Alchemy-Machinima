/**
 * @file llfloaterwateradjust.h
 * @brief Free-floating live water editor with local Windlight preset support.
 *
 * [BDMerge B13] Ported from Black Dragon (donor:
 * I:\black-dragon\indra\newview\llfloaterwateradjust.h, NiranV Dean).
 * Preset routing adapted from gDragonLibrary to gBDMergeEnvLibrary.
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

#ifndef LL_FLOATERWATERADJUST_H
#define LL_FLOATERWATERADJUST_H

#include "llfloater.h"
#include "llsettingsbase.h"
#include "llsettingssky.h"
#include "llenvironment.h"
#include "llflyoutcombobtn.h"

#include "boost/signals2.hpp"

class LLButton;
class LLLineEditor;
class LLColorSwatchCtrl;
class LLTextureCtrl;
class LLComboBox;

/**
 * Floater container for taking a snapshot of the current water and making
 * live adjustments, with disk/inventory preset load & save.
 */
class LLFloaterWaterAdjust : public LLFloater
{
    LOG_CLASS(LLFloaterWaterAdjust);

public:
                                LLFloaterWaterAdjust(const LLSD &key);
    virtual                     ~LLFloaterWaterAdjust();


    virtual bool                postBuild() override;
    virtual void                onOpen(const LLSD& key) override;
    virtual void                onClose(bool app_quitting) override;

    virtual void                refresh() override;

private:
    void                        captureCurrentEnvironment();

    void                        onEnvironmentUpdated(LLEnvironment::EnvSelection_t env, S32 version);

    void                        onButtonApply(LLUICtrl *ctrl, const LLSD &data);
    void                        onSaveAsCommit(const LLSD& notification, const LLSD& response, const LLSettingsBase::ptr_t &settings);
    void                        onInventoryCreated(LLUUID asset_id, LLUUID inventory_id, LLSD results);
    virtual void                doApplyCreateNewInventory(std::string settings_name, const LLSettingsBase::ptr_t &settings);

    //BD - Settings
    void                        onFogColorChanged();
    void                        onFogDensityChanged();
    void                        onFogUnderWaterChanged();

    void                        onNormalScaleChanged();
    void                        onFresnelScaleChanged();
    void                        onFresnelOffsetChanged();
    void                        onScaleAboveChanged();
    void                        onScaleBelowChanged();
    void                        onBlurMultipChanged();

    //BD - Image
    void                        onNormalMapChanged();

    void                        onLargeWaveChanged();
    void                        onSmallWaveChanged();

    //BD - Windlight Stuff
    void                        onButtonReset();
    void                        onButtonSave();
    void                        onButtonDelete();
    void                        onButtonImport();
    void                        onSelectPreset();

    void                        loadWaterSettingFromFile(const std::vector<std::string>& filenames);

    LLSettingsWater::ptr_t      mLiveWater;
    LLEnvironment::connection_t mEventConnection;

    bool                        mWaterImageChanged;

    //BD - Settings
    LLColorSwatchCtrl *         mClrFogColor;
    LLUICtrl*                   mFogDensity;
    LLUICtrl*                   mUnderwaterMod;
    LLUICtrl*                   mWaterNormX;
    LLUICtrl*                   mWaterNormY;
    LLUICtrl*                   mWaterNormZ;
    LLUICtrl*                   mFresnelScale;
    LLUICtrl*                   mFresnelOffset;
    LLUICtrl*                   mScaleAbove;
    LLUICtrl*                   mScaleBelow;
    LLUICtrl*                   mBlurMult;

    //BD - Image
    LLTextureCtrl *             mTxtNormalMap;
    LLUICtrl*                   mWave1X;
    LLUICtrl*                   mWave1Y;
    LLUICtrl*                   mWave2X;
    LLUICtrl*                   mWave2Y;

    LLComboBox*                 mNameCombo;
    LLFlyoutComboBtnCtrl *      mFlyoutControl;
};

#endif // LL_FLOATERWATERADJUST_H
