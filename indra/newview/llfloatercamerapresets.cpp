/**
* @file llfloatercamerapresets.cpp
*
* $LicenseInfo:firstyear=2019&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2019, Linden Research, Inc.
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
// [BDMerge B2] Rename UI for camera preset list items — donor: Black Dragon
// "Unlimited Camera Presets" UX (commit 152762d400). Gated by
// BDMergeCameraPresets; hidden when the gate is off.
#include "llviewerprecompiledheaders.h"

#include "llfloatercamera.h"
#include "llfloatercamerapresets.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llpresetsmanager.h"
#include "llviewercontrol.h"

LLFloaterCameraPresets::LLFloaterCameraPresets(const LLSD& key)
:   LLFloater(key)
{}

LLFloaterCameraPresets::~LLFloaterCameraPresets()
{}

bool LLFloaterCameraPresets::postBuild()
{
    mPresetList = getChild<LLFlatListView>("preset_list");
    mPresetList->setCommitCallback(boost::bind(&LLFloaterCameraPresets::onSelectionChange, this));
    mPresetList->setCommitOnSelectionChange(true);
    LLPresetsManager::getInstance()->setPresetListChangeCameraCallback(boost::bind(&LLFloaterCameraPresets::populateList, this));

    return true;
}
void LLFloaterCameraPresets::onOpen(const LLSD& key)
{
    populateList();
}

void LLFloaterCameraPresets::populateList()
{
    mPresetList->clear();

    LLPresetsManager* presetsMgr = LLPresetsManager::getInstance();
    std::list<std::string> preset_names;

    presetsMgr->loadPresetNamesFromDir(PRESETS_CAMERA, preset_names, DEFAULT_BOTTOM);
    std::string active_preset = gSavedSettings.getString("PresetCameraActive");

    for (std::list<std::string>::const_iterator it = preset_names.begin(); it != preset_names.end(); ++it)
    {
        const std::string& name = *it;
        bool is_default = presetsMgr->isDefaultCameraPreset(name);
        LLCameraPresetFlatItem* item = new LLCameraPresetFlatItem(name, is_default);
        item->postBuild();
        mPresetList->addItem(item);
        if(name == active_preset)
        {
            mPresetList->selectItem(item);
        }
    }
}

void LLFloaterCameraPresets::onSelectionChange()
{
    LLCameraPresetFlatItem* selected_preset = dynamic_cast<LLCameraPresetFlatItem*>(mPresetList->getSelectedItem());
    if(selected_preset)
    {
        LLFloaterCamera::switchToPreset(selected_preset->getPresetName());
    }
}

LLCameraPresetFlatItem::LLCameraPresetFlatItem(const std::string &preset_name, bool is_default)
    : LLPanel(),
    mPresetName(preset_name),
    mIsDefaultPrest(is_default)
{
    mCommitCallbackRegistrar.add("CameraPresets.Delete", boost::bind(&LLCameraPresetFlatItem::onDeleteBtnClick, this));
    mCommitCallbackRegistrar.add("CameraPresets.Reset", boost::bind(&LLCameraPresetFlatItem::onResetBtnClick, this));
    mCommitCallbackRegistrar.add("CameraPresets.Rename", boost::bind(&LLCameraPresetFlatItem::onRenameBtnClick, this)); // [BDMerge B2]
    buildFromFile("panel_camera_preset_item.xml");
}

LLCameraPresetFlatItem::~LLCameraPresetFlatItem()
{
}

bool LLCameraPresetFlatItem::postBuild()
{
    mDeleteBtn = getChild<LLButton>("delete_btn");
    mDeleteBtn->setVisible(false);

    mResetBtn = getChild<LLButton>("reset_btn");
    mResetBtn->setVisible(false);

    // [BDMerge B2] Rename is a BDMergeCameraPresets-gated addition; the button
    // stays hidden entirely (not just on mouse-leave) when the gate is off, so
    // toggling the setting off restores stock Alchemy's list item exactly.
    mRenameBtn = getChild<LLButton>("rename_btn");
    mRenameBtn->setVisible(false);

    LLStyle::Params style;
    LLTextBox* name_text = getChild<LLTextBox>("preset_name");
    LLFontDescriptor new_desc(name_text->getFont()->getFontDesc());
    new_desc.setStyle(mIsDefaultPrest ? LLFontGL::ITALIC : LLFontGL::NORMAL);
    LLFontGL* new_font = LLFontGL::getFont(new_desc);
    style.font = new_font;
    name_text->setText(mPresetName, style);

    return true;
}

void LLCameraPresetFlatItem::onMouseEnter(S32 x, S32 y, MASK mask)
{
    mDeleteBtn->setVisible(!mIsDefaultPrest);
    mResetBtn->setVisible(mIsDefaultPrest);
    // [BDMerge B2] Rename only makes sense for non-default (renameable) presets.
    static LLCachedControl<bool> merge_camera_presets(gSavedSettings, "BDMergeCameraPresets", false);
    mRenameBtn->setVisible(!mIsDefaultPrest && merge_camera_presets);
    getChildView("hovered_icon")->setVisible(true);
    LLPanel::onMouseEnter(x, y, mask);
}

void LLCameraPresetFlatItem::onMouseLeave(S32 x, S32 y, MASK mask)
{
    mDeleteBtn->setVisible(false);
    mResetBtn->setVisible(false);
    mRenameBtn->setVisible(false); // [BDMerge B2]
    getChildView("hovered_icon")->setVisible(false);
    LLPanel::onMouseLeave(x, y, mask);
}

void LLCameraPresetFlatItem::setValue(const LLSD& value)
{
    if (!value.isMap()) return;;
    if (!value.has("selected")) return;
    getChildView("selected_icon")->setVisible(value["selected"]);
}

void LLCameraPresetFlatItem::onDeleteBtnClick()
{
    if (!LLPresetsManager::getInstance()->deletePreset(PRESETS_CAMERA, mPresetName))
    {
        LLSD args;
        args["NAME"] = mPresetName;
        LLNotificationsUtil::add("PresetNotDeleted", args);
    }
}

void LLCameraPresetFlatItem::onResetBtnClick()
{
    LLPresetsManager::getInstance()->resetCameraPreset(mPresetName);
}

// [BDMerge B2]
void LLCameraPresetFlatItem::onRenameBtnClick()
{
    LLSD args;
    args["NAME"] = mPresetName;

    LLSD payload;
    payload["old_name"] = mPresetName;

    LLNotificationsUtil::add("RenameCameraPreset", args, payload, boost::bind(&LLCameraPresetFlatItem::onRenameConfirm, _1, _2));
}

// static
// [BDMerge B2]
void LLCameraPresetFlatItem::onRenameConfirm(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option != 0) return; // canceled

    std::string old_name = notification["payload"]["old_name"].asString();
    std::string new_name = response["new_name"].asString();
    LLStringUtil::trim(new_name);

    if (new_name.empty() || new_name == old_name)
    {
        return;
    }

    if (!LLPresetsManager::getInstance()->renameCameraPreset(old_name, new_name))
    {
        LLSD args;
        args["NAME"] = old_name;
        LLNotificationsUtil::add("PresetNotRenamed", args);
    }
}
