/**
 * @file alfloaterenvintensitypresets.cpp
 * @brief [EnvIntensity userpresets] Save / Load / Delete floater for the Personal
 *        Lighting "Light Intensity" strip presets. See alfloaterenvintensitypresets.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloaterenvintensitypresets.h"

#include "alenvintensitypresets.h"
#include "llbutton.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"

#include <vector>

namespace
{
    // Shows the notification that matches a failed save; nothing for SAVE_OK.
    void envIntensityNotifySave(ALEnvIntensityPresets::SaveResult result, const std::string& name)
    {
        switch (result)
        {
        case ALEnvIntensityPresets::SAVE_INVALID_NAME:
            LLNotificationsUtil::add("EnvIntensityPresetInvalidName");
            break;
        case ALEnvIntensityPresets::SAVE_RESERVED_NAME:
            LLNotificationsUtil::add("EnvIntensityPresetReservedName", LLSD().with("NAME", name));
            break;
        case ALEnvIntensityPresets::SAVE_LIMIT_REACHED:
            LLNotificationsUtil::add("EnvIntensityPresetLimit");
            break;
        case ALEnvIntensityPresets::SAVE_WRITE_FAILED:
            LLNotificationsUtil::add("EnvIntensityPresetSaveFailed");
            break;
        case ALEnvIntensityPresets::SAVE_OK:
            break;
        }
    }

    // Captures the live 16 values (at the moment of the call, i.e. after any
    // overwrite confirmation) under `name`.
    void envIntensityDoSave(const std::string& name)
    {
        envIntensityNotifySave(ALEnvIntensityPresets::saveCurrent(name), name);
    }
}

ALFloaterEnvIntensityPresets::ALFloaterEnvIntensityPresets(const LLSD& key)
:   LLFloater(key)
{
}

ALFloaterEnvIntensityPresets::~ALFloaterEnvIntensityPresets()
{
}

bool ALFloaterEnvIntensityPresets::postBuild()
{
    mList         = getChild<LLScrollListCtrl>("preset_list");
    mNameEditor   = getChild<LLLineEditor>("preset_name");
    mLoadButton   = getChild<LLButton>("btn_load");
    mDeleteButton = getChild<LLButton>("btn_delete");

    mList->setCommitOnSelectionChange(true);
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSelectionChanged(); });
    mList->setDoubleClickCallback([this]() { onLoad(); });

    mNameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSave(); });

    getChild<LLButton>("btn_save")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSave(); });
    mLoadButton->setCommitCallback([this](LLUICtrl*, const LLSD&) { onLoad(); });
    mDeleteButton->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDelete(); });
    getChild<LLButton>("btn_close")->setCommitCallback([this](LLUICtrl*, const LLSD&) { closeFloater(); });

    // Save / overwrite / delete from anywhere refreshes the list; the editor text
    // (the name just saved) is re-selected. Scoped: disconnects with the floater.
    mListChangedConn = ALEnvIntensityPresets::connectListChanged(
        [this]() { refreshList(mNameEditor->getText()); });

    refreshList(std::string());
    return true;
}

void ALFloaterEnvIntensityPresets::onOpen(const LLSD& key)
{
    // Pre-select (and pre-fill) the user preset the live values currently match.
    const std::string match = ALEnvIntensityPresets::findMatchingName(ALEnvIntensityPresets::currentValues());
    refreshList(match);
    if (!match.empty())
    {
        mNameEditor->setText(match);
    }
}

void ALFloaterEnvIntensityPresets::refreshList(std::string select_name)
{
    mList->deleteAllItems();
    const std::vector<std::string> names = ALEnvIntensityPresets::listNames();
    for (const std::string& name : names)
    {
        mList->addSimpleElement(name, ADD_BOTTOM, LLSD(name));
    }
    if (!select_name.empty())
    {
        mList->selectByValue(LLSD(select_name));
    }
    refreshButtons();
}

void ALFloaterEnvIntensityPresets::refreshButtons()
{
    const bool has_selection = !selectedName().empty();
    mLoadButton->setEnabled(has_selection);
    mDeleteButton->setEnabled(has_selection);
}

std::string ALFloaterEnvIntensityPresets::selectedName() const
{
    return mList->getSelectedValue().asString();
}

void ALFloaterEnvIntensityPresets::onSelectionChanged()
{
    const std::string name = selectedName();
    if (!name.empty())
    {
        mNameEditor->setText(name);
    }
    refreshButtons();
}

void ALFloaterEnvIntensityPresets::onSave()
{
    std::string name;
    const ALEnvIntensityPresets::NameCheck check = ALEnvIntensityPresets::checkName(mNameEditor->getText(), name);
    if (check == ALEnvIntensityPresets::NAME_INVALID)
    {
        LLNotificationsUtil::add("EnvIntensityPresetInvalidName");
        return;
    }
    if (check == ALEnvIntensityPresets::NAME_RESERVED)
    {
        LLNotificationsUtil::add("EnvIntensityPresetReservedName", LLSD().with("NAME", name));
        return;
    }

    if (ALEnvIntensityPresets::exists(name))
    {
        // The callback captures only the name (never `this`): the floater may be
        // closed and destroyed before the answer arrives.
        LLNotificationsUtil::add("EnvIntensityPresetOverwrite", LLSD().with("NAME", name), LLSD(),
            [name](const LLSD& notification, const LLSD& response)
            {
                if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                {
                    envIntensityDoSave(name);
                }
            });
        return;
    }
    envIntensityDoSave(name);
}

void ALFloaterEnvIntensityPresets::onLoad()
{
    const std::string name = selectedName();
    if (name.empty())
    {
        return;
    }
    if (!ALEnvIntensityPresets::applyNamed(name))
    {
        // removed since the list was built
        refreshList(std::string());
    }
}

void ALFloaterEnvIntensityPresets::onDelete()
{
    const std::string name = selectedName();
    if (name.empty())
    {
        return;
    }
    LLNotificationsUtil::add("EnvIntensityPresetConfirmDelete", LLSD().with("NAME", name), LLSD(),
        [name](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
            {
                return;
            }
            if (!ALEnvIntensityPresets::remove(name) && ALEnvIntensityPresets::exists(name))
            {
                // still there: the presets file could not be rewritten
                LLNotificationsUtil::add("EnvIntensityPresetSaveFailed");
            }
        });
}
