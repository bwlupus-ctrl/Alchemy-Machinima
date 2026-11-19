/**
 * @file alfloaterenvintensitypresets.h
 * @brief [EnvIntensity userpresets] Save / Load / Delete floater for the Personal
 *        Lighting "Light Intensity" strip presets.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Scroll list of the user presets (ALEnvIntensityPresets), a name line editor and
 * Save / Load / Delete / Close buttons. Save over an existing name asks to
 * overwrite; Delete asks to confirm; double-click on a row loads it. All state
 * lives in ALEnvIntensityPresets, so the list refreshes from its "list changed"
 * signal (also when the combo on the Light Intensity strip is not involved).
 */

#pragma once

#ifndef AL_FLOATERENVINTENSITYPRESETS_H
#define AL_FLOATERENVINTENSITYPRESETS_H

#include "llfloater.h"

#include <boost/signals2.hpp>

#include <string>

class LLButton;
class LLLineEditor;
class LLScrollListCtrl;

class ALFloaterEnvIntensityPresets final : public LLFloater
{
public:
    explicit ALFloaterEnvIntensityPresets(const LLSD& key);
    ~ALFloaterEnvIntensityPresets() override;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

private:
    void refreshList(std::string select_name);   // by value: callers may pass the editor's own text
    void refreshButtons();
    std::string selectedName() const;

    void onSelectionChanged();
    void onSave();
    void onLoad();
    void onDelete();

    LLScrollListCtrl* mList       = nullptr;
    LLLineEditor*     mNameEditor = nullptr;
    LLButton*         mLoadButton   = nullptr;
    LLButton*         mDeleteButton = nullptr;

    boost::signals2::scoped_connection mListChangedConn;
};

#endif // AL_FLOATERENVINTENSITYPRESETS_H
