/**
 * @file bdmergeenvlibrary.h
 * @brief [BDMerge B13] Local Windlight/EEP preset library.
 *
 * Extracted from Black Dragon's BDFunctions (bdfunctions.cpp/h) Windlight
 * cluster: disk-preset save/load/delete under the user and system
 * windlight/{skies,water,days} directories, inventory-preset combo
 * population, and preset selection/apply helpers. Only the environment
 * cluster is ported; the rest of BDFunctions (updater, UI helpers,
 * factory reset, balance) is intentionally NOT here.
 *
 * Donor: I:\black-dragon\indra\newview\bdfunctions.cpp (NiranV Dean).
 * escapeString derives from Singularity Viewer.
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

#ifndef BDMERGE_ENVLIBRARY_H
#define BDMERGE_ENVLIBRARY_H

#include <functional>
#include <string>

#include "llsd.h"
#include "llsettingsbase.h"
#include "lluuid.h"

class LLComboBox;

class BDMergeEnvLibrary
{
public:
    //BD - Windlight functions
    void savePreset(std::string name, LLSettingsBase::ptr_t settings);
    //BD - [FileSafety] Deletes exactly one local preset file (never a glob). Returns true if removed.
    bool deletePreset(std::string name, std::string folder = "skies");
    //BD - [FileSafety] Ask for confirmation, then deletePreset(); on_done runs after the
    //     attempt (success or not) so callers can refresh their lists. Not called on Cancel.
    void confirmDeletePreset(std::string name, std::string folder, std::function<void()> on_done);
    void loadPresetsFromDir(LLComboBox* combo, std::string folder = "skies");
    bool doLoadPreset(const std::string& path);
    static std::string getWindlightDir(std::string folder, bool system = false);
    bool checkPermissions(LLUUID uuid);
    void onSelectPreset(LLComboBox* combo, LLSettingsBase::ptr_t settings);
    void addInventoryPresets(LLComboBox* combo, LLSettingsBase::ptr_t settings);
    void loadItem(LLSettingsBase::ptr_t settings);
    bool loadPreset(std::string filename, LLSettingsBase::ptr_t settings);

    //BD - Escape string (Singularity-derived hex escaper; encodes '-' too,
    //     unlike LLURI::escape())
    static std::string escapeString(const std::string& str);

    LLSD mDefaultSkyPresets;
    LLSD mDefaultWaterPresets;
    LLSD mDefaultDayCyclePresets;
};

extern BDMergeEnvLibrary gBDMergeEnvLibrary;

#endif // BDMERGE_ENVLIBRARY_H
