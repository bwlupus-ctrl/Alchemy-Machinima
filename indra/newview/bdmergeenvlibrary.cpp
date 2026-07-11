/**
 * @file bdmergeenvlibrary.cpp
 * @brief [BDMerge B13] Local Windlight/EEP preset library.
 *
 * Extracted from Black Dragon's BDFunctions (bdfunctions.cpp) Windlight
 * cluster. See bdmergeenvlibrary.h for scope and provenance notes.
 *
 * Donor: I:\black-dragon\indra\newview\bdfunctions.cpp (NiranV Dean).
 *
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
 */

#include "llviewerprecompiledheaders.h"

#include "bdmergeenvlibrary.h"

#include "llagent.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llenvironment.h"
#include "llfile.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorysettings.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llsettingsvo.h"
#include "lluri.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"

BDMergeEnvLibrary gBDMergeEnvLibrary;

//BD - Windlight Stuff
//=====================================================================================================
namespace
{
    // Used for sorting
    struct SortItemPtrsByName
    {
        bool operator()(const LLInventoryItem* i1, const LLInventoryItem* i2)
        {
            return (LLStringUtil::compareDict(i1->getName(), i2->getName()) < 0);
        }
    };

    // [BDMerge B13] Alchemy's LLComboBox::addSeparator() takes no label (BD
    // extended its combobox with labeled separators); approximate with a
    // separator plus a disabled caption row.
    void add_labeled_separator(LLComboBox* combo, EAddPosition pos, const std::string& label)
    {
        combo->addSeparator(pos);
        combo->add(label, LLSD(), pos, false);
    }
}

//BD - Escape string
std::string BDMergeEnvLibrary::escapeString(const std::string& str)
{
    //BD - Don't use LLURI::escape() because it doesn't encode '-' characters
    //     which may break handling of some poses.
    //     From Singularity Viewer.
    static const char hex[] = "0123456789ABCDEF";
    std::stringstream escaped_str;
    for (std::string::const_iterator iter = str.begin(); iter != str.end(); ++iter)
    {
        switch (*iter) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
            escaped_str << (*iter);
            break;
        default:
            unsigned char c = (unsigned char)(*iter);
            escaped_str << '%' << hex[c >> 4] << hex[c & 0xF];
        }
    }
    return escaped_str.str();
}

bool BDMergeEnvLibrary::checkPermissions(LLUUID uuid)
{
    LLViewerInventoryItem *item = gInventory.getItem(uuid);
    if (item)
    {
        LLPermissions perms = item->getPermissions();
        if (perms.allowOperationBy(PERM_TRANSFER, gAgent.getID())
            && perms.allowOperationBy(PERM_COPY, gAgent.getID()))
        {
            return true;
        }
    }

    return false;
}

void BDMergeEnvLibrary::addInventoryPresets(LLComboBox* combo, LLSettingsBase::ptr_t settings)
{
    if (!combo || !settings) return;

    std::string type_str = settings->getSettingsType();
    LLSettingsType::type_e type = type_str == "sky" ? LLSettingsType::ST_SKY : type_str == "water" ? LLSettingsType::ST_WATER : LLSettingsType::ST_DAYCYCLE;

    // Get all inventory items that are settings
    LLViewerInventoryCategory::cat_array_t cats;
    LLViewerInventoryItem::item_array_t items;
    LLIsTypeWithPermissions is_copyable_animation(LLAssetType::AT_SETTINGS,
        PERM_TRANSFER,
        gAgent.getID(),
        gAgent.getGroupID());
    gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
        cats,
        items,
        LLInventoryModel::EXCLUDE_TRASH,
        is_copyable_animation);

    //BD - Copy into something we can sort
    std::vector<LLViewerInventoryItem*> presets;

    size_t count = items.size();
    for (size_t i = 0; i < count; ++i)
    {
        presets.push_back(items.at(i));
    }

    //BD - Do the sort
    std::sort(presets.begin(), presets.end(), SortItemPtrsByName());

    //BD - Add the inventory presets separator if we found some.
    if (count > 0)
    {
        add_labeled_separator(combo, ADD_BOTTOM, "Inventory Presets");
    }

    std::vector<LLViewerInventoryItem*>::iterator it;
    for (it = presets.begin(); it != presets.end(); ++it)
    {
        LLViewerInventoryItem* item = *it;
        if (item->getSettingsType() == type)
        {
            combo->add(item->getName(), item->getUUID(), ADD_BOTTOM);
        }
    }
}

void BDMergeEnvLibrary::onSelectPreset(LLComboBox* combo, LLSettingsBase::ptr_t settings)
{
    if (!combo || !settings) return;

    //BD - First attempt to load it as inventory item.
    if (combo->getValue().isUUID())
    {
        LLUUID uuid = combo->getValue();
        LLViewerInventoryItem* item = gInventory.getItem(uuid);
        if (item)
        {
            LLSettingsVOBase::getSettingsAsset(item->getAssetUUID(), [this](LLUUID asset_id, LLSettingsBase::ptr_t settings, S32 status, LLExtStat) { loadItem(settings); });
            //BD - Assume loading was successful.
            return;
        }
    }

    //BD - Loading as inventory item failed so it must be a local preset.
    std::string name = combo->getValue().asString();

    if (!loadPreset(name, settings))
    {
        LLNotificationsUtil::add("BDCantLoadPreset");
        LL_WARNS("Windlight") << "Failed to load windlight preset from: " << name << LL_ENDL;
    }
}

void BDMergeEnvLibrary::loadItem(LLSettingsBase::ptr_t settings)
{
    if (!settings) return;

    LLEnvironment &env(LLEnvironment::instance());
    std::string type = settings->getSettingsType();
    if (type == "sky")
        env.setEnvironment(LLEnvironment::ENV_LOCAL, std::static_pointer_cast<LLSettingsSky>(settings));
    else if (type == "daycycle")
        env.setEnvironment(LLEnvironment::ENV_LOCAL, std::static_pointer_cast<LLSettingsDay>(settings));
    else if (type == "water")
        env.setEnvironment(LLEnvironment::ENV_LOCAL, std::static_pointer_cast<LLSettingsWater>(settings));
    env.updateEnvironment(LLSettingsBase::Seconds(gSavedSettings.getF32("RenderWindlightInterpolateTime")));
}

bool BDMergeEnvLibrary::loadPreset(std::string filename, LLSettingsBase::ptr_t settings)
{
    if (!settings || filename.empty()) return false;

    //BD - If we get here we are loading a local preset which we assume allows us to save.
    LLEnvironment::instance().setLocalPreset(true);

    llifstream xml_file;
    xml_file.open(filename.c_str());
    if (!xml_file) return false;

    LLSD params_data;
    LLPointer<LLSDParser> parser = new LLSDXMLParser();
    if (parser->parse(xml_file, params_data, LLSDSerialize::SIZE_UNLIMITED) == LLSDParser::PARSE_FAILURE)
    {
        xml_file.close();
        LLNotificationsUtil::add("BDCantParsePreset");
        return false;
    }
    xml_file.close();

    LLEnvironment &env(LLEnvironment::instance());
    LLSD messages;
    std::string type = settings->getSettingsType();
    if (type == "sky")
        settings = !params_data.has("version") ? env.createSkyFromLegacyPreset(filename, messages) : env.createSkyFromPreset(filename, messages);
    else if (type == "daycycle")
        settings = !params_data.has("version") ? env.createDayCycleFromLegacyPreset(filename, messages) : env.createDayCycleFromPreset(filename, messages);
    else if (type == "water")
        settings = !params_data.has("version") ? env.createWaterFromLegacyPreset(filename, messages) : env.createWaterFromPreset(filename, messages);

    if (!settings)
    {
        LLNotificationsUtil::add("WLImportFail", messages);
        return false;
    }

    env.setEnvironment(LLEnvironment::ENV_LOCAL, settings, -2);
    env.setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
    env.updateEnvironment(LLSettingsBase::Seconds(gSavedSettings.getF32("RenderWindlightInterpolateTime")));

    return true;
}

//BD - Windlight functions
void BDMergeEnvLibrary::savePreset(std::string name, LLSettingsBase::ptr_t settings)
{
    if (!settings || name.empty()) return;

    LLSD Params = settings->getSettings();
    std::string type = settings->getSettingsType();
    std::string folder = type == "sky" ? "skies" : type == "water" ? "water" : "days";

    //BD - Make sure whatever string we get is a name only and doesn't contain a file ending.
    //     Next make sure whatever string we get is unescaped.
    name = gDirUtilp->getBaseFileName(LLURI::unescape(name), true);

    // make an empty llsd
    std::string pathName(getWindlightDir(folder) + escapeString(name) + ".xml");

    Params["version"] = "eep";

    // write to file
    llofstream presetsXML(pathName.c_str());
    LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
    formatter->format(Params, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
    presetsXML.close();
}

void BDMergeEnvLibrary::deletePreset(std::string name, std::string folder)
{
    if (name.empty() || folder.empty()) return;

    //BD - Check whether we are in the system presets folder which means we are trying
    //     to delete one.
    std::string filename = gDirUtilp->getBaseFileName(name, false);
    std::string sys_dir = getWindlightDir(folder, true);
    gDirUtilp->append(sys_dir, filename);

    // Don't allow deleting system presets.
    if (sys_dir == name)
    {
        LLNotificationsUtil::add("WLNoEditDefault");
        return;
    }
    else
    {
        //BD - We assume its the user_settings folder since we never allow deleting
        //     system presets and we shouldn't get here if we intended to delete one.
        if (gDirUtilp->deleteFilesInDir(getWindlightDir(folder), filename) < 1)
        {
            LLNotificationsUtil::add("BDCantRemovePreset");
            LL_WARNS("WindLight") << "Error removing windlight preset " << name << " from disk" << LL_ENDL;
        }
    }
}

void BDMergeEnvLibrary::loadPresetsFromDir(LLComboBox* combo, std::string folder)
{
    if (!combo || folder.empty()) return;

    bool success = false;
    combo->clearRows();

    std::string dir = getWindlightDir(folder);
    std::string file;
    if (!dir.empty() || gDirUtilp->fileExists(dir))
    {
        LLDirIterator dir_it(dir, "*.xml");
        while (dir_it.next(file))
        {
            std::string path = gDirUtilp->add(dir, file);
            std::string name = gDirUtilp->getBaseFileName(LLURI::unescape(path), true);

            //BD - Skip adding if we couldn't load it.
            if (!doLoadPreset(path))
            {
                LL_WARNS() << "Error loading windlight preset from: " << path << LL_ENDL;
                continue;
            }

            combo->add(name, LLSD(path), ADD_BOTTOM, true);
            success = true;
        }
    }

    //BD - Add the user presets separator if we found user presets.
    if (success)
    {
        add_labeled_separator(combo, ADD_TOP, "User Presets");
    }

    //BD - We assume they are always there.
    add_labeled_separator(combo, ADD_BOTTOM, "System Presets");

    dir = getWindlightDir(folder, true);
    if (!dir.empty() || gDirUtilp->fileExists(dir))
    {
        LLDirIterator dir_iter(dir, "*.xml");
        while (dir_iter.next(file))
        {
            std::string path = gDirUtilp->add(dir, file);
            std::string name = gDirUtilp->getBaseFileName(LLURI::unescape(path), true);

            //BD - Skip adding if we couldn't load it.
            if (!doLoadPreset(path))
            {
                LL_WARNS() << "Error loading windlight preset from: " << path << LL_ENDL;
                continue;
            }

            combo->add(name, LLSD(path), ADD_BOTTOM, true);
            if (folder == "skies")
                mDefaultSkyPresets[name] = name;
            else if (folder == "days")
                mDefaultDayCyclePresets[name] = name;
            else if (folder == "water")
                mDefaultWaterPresets[name] = name;
        }
    }
}

bool BDMergeEnvLibrary::doLoadPreset(const std::string& path)
{
    llifstream xml_file;

    xml_file.open(path.c_str());
    if (!xml_file)
    {
        return false;
    }

    LLSD params_data;
    LLPointer<LLSDParser> parser = new LLSDXMLParser();
    parser->parse(xml_file, params_data, LLSDSerialize::SIZE_UNLIMITED);
    xml_file.close();

    return true;
}

//BD - Multiple Viewer Presets
// static
std::string BDMergeEnvLibrary::getWindlightDir(std::string folder, bool system)
{
    if (folder.empty()) return std::string();

    ELLPath path_enum = system ? LL_PATH_APP_SETTINGS : LL_PATH_USER_SETTINGS;

    //BD - Check for the top level folder first
    std::string sys_dir = gDirUtilp->getExpandedFilename(path_enum, "windlight");
    if (!gDirUtilp->fileExists(sys_dir))
    {
        LLFile::mkdir(sys_dir);
    }

    sys_dir = gDirUtilp->getExpandedFilename(path_enum, "windlight", folder);
    if (!gDirUtilp->fileExists(sys_dir))
    {
        LL_WARNS("Windlight") << "Couldn't find folder: " << sys_dir << " - creating one." << LL_ENDL;
        LLFile::mkdir(sys_dir);
    }

    sys_dir = gDirUtilp->getExpandedFilename(path_enum, "windlight", folder, "");
    return sys_dir;
}
