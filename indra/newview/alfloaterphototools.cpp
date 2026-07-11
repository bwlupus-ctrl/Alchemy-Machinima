/**
 * @file alfloaterphototools.cpp
 * @brief Phototools: EEP environment quick-switcher, DoF focus point
 * lock/follow/crosshair, and rule-of-thirds/golden-ratio composition guide
 * overlay.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 * Copyright (C) 2013, Cinder Biscuits @ Me too
 * Depth-of-field WYSIWYG fix, focus point lock/crosshair, and composition
 * guide overlay by William Weaver ("paperwork").
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 *
 * Ported from Firestorm's QuickPrefs/Phototools floater (donor: I:\enve
 * indra/newview/quickprefs.cpp, FloaterQuickPrefs::{loadPresets,
 * loadSkyPresets, loadWaterPresets, loadDayCyclePresets, setSelectedEnvironment,
 * isValidPreset, stepComboBox, select*Preset, onChange*Preset, onClick*Prev/Next,
 * onClickResetToRegionDefault}) for the BD/Alchemy merge campaign, item F4
 * (+F8). API mismatch resolved: donor calls a convenience
 * `LLEnvironment::setManualEnvironment(env, uuid)` that does not exist in
 * this fork's llenvironment.h; replaced with the
 * setEnvironment(env, assetId, transition) + setSelectedEnvironment(env, transition)
 * pair already used the same way by llinventorybridge.cpp
 * LLSettingsBridge::performAction("apply_settings_local") and
 * llfloatermyenvironment.cpp. OpenSim legacy-windlight branches (#ifdef
 * OPENSIM in the donor) are dropped; this fork does not build with OPENSIM.
 */

#include "llviewerprecompiledheaders.h"
#include "alfloaterphototools.h"

#include "llagent.h"
#include "llcombobox.h"
#include "llenvironment.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llnotificationsutil.h"
#include "llsettingsdaycycle.h"
#include "llsettingssky.h"
#include "llsettingswater.h"
#include "lltrans.h"
#include "llviewerinventory.h"

namespace
{
    const std::string PRESET_NAME_REGION_DEFAULT("__Regiondefault__");
    const std::string PRESET_NAME_DAY_CYCLE("__Day_Cycle__");
    const std::string PRESET_NAME_NONE("__None__");

    // donor: quickprefs.cpp FSSettingsCollector - collects AT_SETTINGS
    // inventory items, skipping Marketplace listings and duplicate assets.
    class PhototoolsSettingsCollector : public LLInventoryCollectFunctor
    {
    public:
        PhototoolsSettingsCollector()
        {
            mMarketplaceFolderUUID = gInventory.getMarketplaceListingsUUID();
        }

        bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override
        {
            if (item && item->getType() == LLAssetType::AT_SETTINGS &&
                !gInventory.isObjectDescendentOf(item->getUUID(), mMarketplaceFolderUUID) &&
                mSeen.find(item->getAssetUUID()) == mSeen.end())
            {
                mSeen.insert(item->getAssetUUID());
                return true;
            }
            return false;
        }

    private:
        LLUUID mMarketplaceFolderUUID;
        uuid_set_t mSeen;
    };
}

ALFloaterPhototools::ALFloaterPhototools(const LLSD& key)
:   LLFloater(key)
{
}

bool ALFloaterPhototools::postBuild()
{
    mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
    mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
    mDayCyclePresetsCombo = getChild<LLComboBox>("DCPresetsCombo");

    mWLPresetsCombo->setCommitCallback(boost::bind(&ALFloaterPhototools::onChangeSkyPreset, this));
    mWaterPresetsCombo->setCommitCallback(boost::bind(&ALFloaterPhototools::onChangeWaterPreset, this));
    mDayCyclePresetsCombo->setCommitCallback(boost::bind(&ALFloaterPhototools::onChangeDayCyclePreset, this));

    getChild<LLUICtrl>("WLPrevPreset")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickSkyPrev, this));
    getChild<LLUICtrl>("WLNextPreset")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickSkyNext, this));
    getChild<LLUICtrl>("WWPrevPreset")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickWaterPrev, this));
    getChild<LLUICtrl>("WWNextPreset")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickWaterNext, this));
    getChild<LLUICtrl>("DCPrevPreset")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickDayCyclePrev, this));
    getChild<LLUICtrl>("DCNextPreset")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickDayCycleNext, this));
    getChild<LLUICtrl>("ResetToRegionDefault")->setCommitCallback(boost::bind(&ALFloaterPhototools::onClickResetToRegionDefault, this));

    mEnvChangedConnection = LLEnvironment::instance().setEnvironmentChanged(
        [this](LLEnvironment::EnvSelection_t env, S32 version) { setSelectedEnvironment(); });

    return LLFloater::postBuild();
}

void ALFloaterPhototools::onOpen(const LLSD& key)
{
    loadPresets();
    setSelectedEnvironment();
}

// -------------------------------------------------------------------------
// EEP environment quick-switcher
// -------------------------------------------------------------------------

void ALFloaterPhototools::loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map)
{
    mDayCyclePresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), ADD_BOTTOM, false);
    mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_None"), LLSD(PRESET_NAME_NONE), ADD_BOTTOM, false);
    mDayCyclePresetsCombo->addSeparator();

    for (const auto& [preset_name, asset_id] : daycycle_map)
    {
        if (!preset_name.empty())
        {
            mDayCyclePresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
}

void ALFloaterPhototools::loadSkyPresets(const std::multimap<std::string, LLUUID>& sky_map)
{
    mWLPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mWLPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), ADD_BOTTOM, false);
    mWLPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_DAY_CYCLE), ADD_BOTTOM, false);
    mWLPresetsCombo->addSeparator();

    for (const auto& [preset_name, asset_id] : sky_map)
    {
        if (!preset_name.empty())
        {
            mWLPresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
}

void ALFloaterPhototools::loadWaterPresets(const std::multimap<std::string, LLUUID>& water_map)
{
    mWaterPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), ADD_BOTTOM, false);
    mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_DAY_CYCLE), ADD_BOTTOM, false);
    mWaterPresetsCombo->addSeparator();

    for (const auto& [preset_name, asset_id] : water_map)
    {
        if (!preset_name.empty())
        {
            mWaterPresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
}

void ALFloaterPhototools::loadPresets()
{
    LLInventoryModel::cat_array_t cats;
    LLInventoryModel::item_array_t items;
    PhototoolsSettingsCollector collector;
    gInventory.collectDescendentsIf(LLUUID::null, cats, items, LLInventoryModel::EXCLUDE_TRASH, collector);

    std::multimap<std::string, LLUUID> sky_map;
    std::multimap<std::string, LLUUID> water_map;
    std::multimap<std::string, LLUUID> daycycle_map;

    for (const auto& item : items)
    {
        switch (LLSettingsType::fromInventoryFlags(item->getFlags()))
        {
            case LLSettingsType::ST_SKY:
                sky_map.emplace(item->getName(), item->getAssetUUID());
                break;
            case LLSettingsType::ST_WATER:
                water_map.emplace(item->getName(), item->getAssetUUID());
                break;
            case LLSettingsType::ST_DAYCYCLE:
                daycycle_map.emplace(item->getName(), item->getAssetUUID());
                break;
            default:
                LL_WARNS("Phototools") << "Found invalid setting: " << item->getName() << LL_ENDL;
                break;
        }
    }

    loadWaterPresets(water_map);
    loadSkyPresets(sky_map);
    loadDayCyclePresets(daycycle_map);
}

// API mismatch resolved (see file header): donor's setDefaultPresetsEnabled()
// dims the "Region Default"/"Day Cycle"/"None" placeholder combo entries via
// LLComboBox::getItemByValue(const LLSD&), which does not exist in this
// fork's llcombobox.h (only findItemByValue(const std::string&) and
// valueExists(const std::string&) do, and neither exposes an LLScrollListItem
// to re-enable/disable by an LLUUID-valued entry). Dropped: it was cosmetic
// dimming only - isValidPreset()/stepComboBox() already skip those
// placeholder entries when stepping through presets regardless of their
// enabled state, so behavior is unaffected, only the visual dimming is gone.

void ALFloaterPhototools::setSelectedEnvironment()
{
    mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));

    if (LLEnvironment::instance().getSelectedEnvironment() == LLEnvironment::ENV_LOCAL)
    {
        if (LLSettingsDay::ptr_t day = LLEnvironment::instance().getEnvironmentDay(LLEnvironment::ENV_LOCAL))
        {
            if (day->getAssetId().notNull())
            {
                mDayCyclePresetsCombo->selectByValue(LLSD(day->getAssetId()));
                mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
                mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
            }
        }
        else
        {
            mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_NONE));
        }

        if (LLSettingsSky::ptr_t sky = LLEnvironment::instance().getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL))
        {
            if (sky->getAssetId().notNull())
            {
                mWLPresetsCombo->selectByValue(LLSD(sky->getAssetId()));
            }
        }
        if (LLSettingsWater::ptr_t water = LLEnvironment::instance().getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL))
        {
            if (water->getAssetId().notNull())
            {
                mWaterPresetsCombo->selectByValue(LLSD(water->getAssetId()));
            }
        }
    }
    else
    {
        // ENV_REGION / ENV_PARCEL
        mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    }
}

bool ALFloaterPhototools::isValidPreset(const LLSD& preset)
{
    if (preset.isUUID())
    {
        return !preset.asUUID().isNull();
    }
    if (preset.isString())
    {
        return !preset.asString().empty() &&
               preset.asString() != PRESET_NAME_REGION_DEFAULT &&
               preset.asString() != PRESET_NAME_DAY_CYCLE &&
               preset.asString() != PRESET_NAME_NONE;
    }
    return false;
}

void ALFloaterPhototools::stepComboBox(LLComboBox* ctrl, bool forward)
{
    S32 increment = forward ? 1 : -1;
    S32 lastitem = ctrl->getItemCount() - 1;
    S32 curid = ctrl->getCurrentIndex();
    S32 startid = curid;

    do
    {
        curid += increment;
        if (curid < 0)
        {
            curid = lastitem;
        }
        else if (curid > lastitem)
        {
            curid = 0;
        }
        ctrl->setCurrentByIndex(curid);
    }
    while (!isValidPreset(ctrl->getSelectedValue()) && curid != startid);
}

// API mismatch resolved (see file header): donor's LLEnvironment::setManualEnvironment(env, uuid)
// does not exist here; use setEnvironment(env, assetId, transition) +
// setSelectedEnvironment(env, transition), matching llinventorybridge.cpp
// LLSettingsBridge::performAction("apply_settings_local").
void ALFloaterPhototools::selectSkyPreset(const LLSD& preset)
{
    auto& instance = LLEnvironment::instance();
    instance.setEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID(), LLEnvironment::TRANSITION_DEFAULT);
    instance.setSelectedEnvironment(LLEnvironment::ENV_LOCAL, LLEnvironment::TRANSITION_DEFAULT);
}

void ALFloaterPhototools::selectWaterPreset(const LLSD& preset)
{
    auto& instance = LLEnvironment::instance();
    instance.setEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID(), LLEnvironment::TRANSITION_DEFAULT);
    instance.setSelectedEnvironment(LLEnvironment::ENV_LOCAL, LLEnvironment::TRANSITION_DEFAULT);
}

void ALFloaterPhototools::selectDayCyclePreset(const LLSD& preset)
{
    auto& instance = LLEnvironment::instance();
    instance.setEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID(), LLEnvironment::TRANSITION_DEFAULT);
    instance.setSelectedEnvironment(LLEnvironment::ENV_LOCAL, LLEnvironment::TRANSITION_DEFAULT);
}

void ALFloaterPhototools::onChangeWaterPreset()
{
    if (!isValidPreset(mWaterPresetsCombo->getSelectedValue()))
    {
        stepComboBox(mWaterPresetsCombo, true);
    }
    if (isValidPreset(mWaterPresetsCombo->getSelectedValue()))
    {
        selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void ALFloaterPhototools::onChangeSkyPreset()
{
    if (!isValidPreset(mWLPresetsCombo->getSelectedValue()))
    {
        stepComboBox(mWLPresetsCombo, true);
    }
    if (isValidPreset(mWLPresetsCombo->getSelectedValue()))
    {
        selectSkyPreset(mWLPresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void ALFloaterPhototools::onChangeDayCyclePreset()
{
    if (!isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    {
        stepComboBox(mDayCyclePresetsCombo, true);
    }
    if (isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    {
        selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void ALFloaterPhototools::onClickWaterPrev()
{
    stepComboBox(mWaterPresetsCombo, false);
    selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
}

void ALFloaterPhototools::onClickWaterNext()
{
    stepComboBox(mWaterPresetsCombo, true);
    selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
}

void ALFloaterPhototools::onClickSkyPrev()
{
    stepComboBox(mWLPresetsCombo, false);
    selectSkyPreset(mWLPresetsCombo->getSelectedValue());
}

void ALFloaterPhototools::onClickSkyNext()
{
    stepComboBox(mWLPresetsCombo, true);
    selectSkyPreset(mWLPresetsCombo->getSelectedValue());
}

void ALFloaterPhototools::onClickDayCyclePrev()
{
    stepComboBox(mDayCyclePresetsCombo, false);
    selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
}

void ALFloaterPhototools::onClickDayCycleNext()
{
    stepComboBox(mDayCyclePresetsCombo, true);
    selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
}

void ALFloaterPhototools::onClickResetToRegionDefault()
{
    mWLPresetsCombo->setValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mWaterPresetsCombo->setValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    LLEnvironment::instance().setSharedEnvironment();
}
