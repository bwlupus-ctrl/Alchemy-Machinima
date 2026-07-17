/**
 * @file alpanelcinecamparams.cpp
 * @brief Cinematic Camera parameter panel -- see alpanelcinecamparams.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelcinecamparams.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfile.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lluri.h"
#include "llviewercontrol.h"        // gSavedSettings

// both the standalone Cinematic Camera floater and the Director Console
// instantiate this class via <panel class="panel_cinecam_params" .../>
static LLPanelInjector<ALPanelCineCamParams> t_panel_cinecam_params("panel_cinecam_params");

namespace
{
constexpr char PRESET_SUBDIR[] = "cinematic_presets";
} // anonymous namespace

// ---------------------------------------------------------------------------
// mode -> panel + settings table
//
// Ground truth is the set of LLCachedControl declarations inside each pattern
// function of llcinematiccamera.cpp. No two modes share a setting, so every
// mode gets its own panel. Keep this table in sync with the camera code.
// ---------------------------------------------------------------------------
//static
const std::vector<ALPanelCineCamParams::ModeEntry>& ALPanelCineCamParams::modeTable()
{
    static const std::vector<ModeEntry> table = {
        {  1, "panel_mode_bone",     { "CinematicCamJoint",
                                       "CinematicCamBoneOffsetForward", "CinematicCamBoneOffsetLeft", "CinematicCamBoneOffsetUp",
                                       "CinematicCamBoneAimYaw", "CinematicCamBoneAimPitch", "CinematicCamBoneAimRoll",
                                       "CinematicCamBoneHorizonLock" } },
        {  2, "panel_mode_orbit",    { "CinematicCamOrbitRadius", "CinematicCamOrbitSpeed",
                                       "CinematicCamOrbitHeight", "CinematicCamOrbitBob" } },
        {  3, "panel_mode_hover",    { "CinematicCamHoverDistance", "CinematicCamHoverWander",
                                       "CinematicCamHoverSpeed", "CinematicCamHoverHeight" } },
        {  4, "panel_mode_sweep",    { "CinematicCamSweepLength", "CinematicCamSweepDistance", "CinematicCamSweepSpeed",
                                       "CinematicCamSweepHeading", "CinematicCamSweepHeight", "CinematicCamSweepPingPong" } },
        {  5, "panel_mode_crane",    { "CinematicCamCraneRadius", "CinematicCamCraneSpeed",
                                       "CinematicCamCraneMinHeight", "CinematicCamCraneMaxHeight",
                                       "CinematicCamCraneRisePeriod" } },
        {  6, "panel_mode_vertigo",  { "CinematicCamVertigoStartDist", "CinematicCamVertigoEndDist",
                                       "CinematicCamVertigoDuration", "CinematicCamVertigoHeading",
                                       "CinematicCamVertigoHeight", "CinematicCamVertigoEndMode" } },
        {  7, "panel_mode_push",     { "CinematicCamPushStartDist", "CinematicCamPushEndDist",
                                       "CinematicCamPushDuration", "CinematicCamPushHeading",
                                       "CinematicCamPushHeight", "CinematicCamPushEndMode" } },
        {  8, "panel_mode_hero",     { "CinematicCamHeroDistance", "CinematicCamHeroHeight", "CinematicCamHeroArc",
                                       "CinematicCamHeroPeriod", "CinematicCamHeroHeading" } },
        {  9, "panel_mode_overhead", { "CinematicCamOverheadStart", "CinematicCamOverheadEnd",
                                       "CinematicCamOverheadDuration", "CinematicCamOverheadSpin",
                                       "CinematicCamOverheadEndMode" } },
        { 10, "panel_mode_ots",      { "CinematicCamOTSSide", "CinematicCamOTSBack",
                                       "CinematicCamOTSOut", "CinematicCamOTSUp" } },
        { 11, "panel_mode_crash",    { "CinematicCamCrashZoom", "CinematicCamCrashDuration" } },
        { 12, "panel_mode_slowzoom", { "CinematicCamSlowZoomTarget", "CinematicCamSlowZoomDuration" } },
        { 13, "panel_mode_whip",     { "CinematicCamWhipFrom", "CinematicCamWhipTo", "CinematicCamWhipDuration",
                                       "CinematicCamWhipDistance", "CinematicCamWhipHeight" } },
        { 14, "panel_mode_arc",      { "CinematicCamArcFrom", "CinematicCamArcTo", "CinematicCamArcDuration",
                                       "CinematicCamArcDistance", "CinematicCamArcHeight", "CinematicCamArcEndMode" } },
        { 15, "panel_mode_reveal",   { "CinematicCamRevealBehind", "CinematicCamRevealLowHeight",
                                       "CinematicCamRevealHighHeight", "CinematicCamRevealAhead",
                                       "CinematicCamRevealDuration" } },
        { 16, "panel_mode_pull",     { "CinematicCamPullStartDist", "CinematicCamPullEndDist",
                                       "CinematicCamPullEndHeight", "CinematicCamPullDuration",
                                       "CinematicCamPullHeading" } },
        { 17, "panel_mode_twoshot",  { "CinematicCamTwoShotSide", "CinematicCamTwoShotPad",
                                       "CinematicCamTwoShotMinDist", "CinematicCamTwoShotHeight" } },
        { 18, "panel_mode_lead",     { "CinematicCamLeadDistance", "CinematicCamLeadHeight",
                                       "CinematicCamLeadSway" } },
        { 19, "panel_mode_ecu",      { "CinematicCamECUDistance", "CinematicCamECUZoom",
                                       "CinematicCamECUDrift" } },
        { 20, "panel_mode_long",     { "CinematicCamLongDistance", "CinematicCamLongZoom", "CinematicCamLongHeading",
                                       "CinematicCamLongHeight", "CinematicCamLongDrift" } },
        { 21, "panel_mode_spiral",   { "CinematicCamSpiralStartRadius", "CinematicCamSpiralEndRadius",
                                       "CinematicCamSpiralStartHeight", "CinematicCamSpiralEndHeight",
                                       "CinematicCamSpiralSpeed", "CinematicCamSpiralDuration" } },
        { 22, "panel_mode_pedestal", { "CinematicCamPedestalDistance", "CinematicCamPedestalStart",
                                       "CinematicCamPedestalEnd", "CinematicCamPedestalDuration",
                                       "CinematicCamPedestalHeading" } },
    };
    return table;
}

//static
const std::vector<std::string>& ALPanelCineCamParams::sharedSettings()
{
    // read for every mode in isActive()/resolveTarget()/updateCamera()
    static const std::vector<std::string> shared = {
        "CinematicCamEnabled",
        "CinematicCamMode",
        "CinematicCamUseSelected",
        "CinematicCamLookAtHead",
        "CinematicCamUseOperator",
        "CinematicCamSmoothing",
        "CinematicCamDutchAngle",
        "CinematicCamFrameOffsetUp",
    };
    return shared;
}

// ---------------------------------------------------------------------------
ALPanelCineCamParams::~ALPanelCineCamParams()
{
    if (mModeConnection.connected())
    {
        mModeConnection.disconnect();
    }
}

bool ALPanelCineCamParams::postBuild()
{
    getChild<LLButton>("btn_reset_mode")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickResetMode(); });
    getChild<LLButton>("btn_reset_all")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickResetAll(); });
    getChild<LLButton>("btn_preset_save")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickSavePreset(); });
    getChild<LLButton>("btn_preset_delete")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickDeletePreset(); });

    mPresetCombo = getChild<LLComboBox>("preset_combo");
    mPresetCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPresetSelected(); });

    // react to mode changes from the header combo AND from anywhere else
    // (debug settings, scripts, another instance of this panel): the
    // control's commit signal covers all of them
    if (LLControlVariable* mode_ctrl = gSavedSettings.getControl("CinematicCamMode"))
    {
        mModeConnection = mode_ctrl->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateModePanel(); });
    }

    updateModePanel();
    refreshPresetList();
    return true;
}

void ALPanelCineCamParams::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateModePanel();
        // keep the current selection while picking up presets saved from
        // another instance since we were last shown
        refreshPresetList(mPresetCombo ? mPresetCombo->getSelectedItemLabel()
                                       : std::string());
    }
    LLPanel::onVisibilityChange(new_visibility);
}

// ---------------------------------------------------------------------------
// per-mode panel visibility
// ---------------------------------------------------------------------------
void ALPanelCineCamParams::updateModePanel()
{
    const S32 mode = gSavedSettings.getS32("CinematicCamMode");
    for (const ModeEntry& entry : modeTable())
    {
        if (LLPanel* panel = findChild<LLPanel>(entry.mPanel))
        {
            panel->setVisible(entry.mMode == mode);
        }
    }
}

// ---------------------------------------------------------------------------
// resets
// ---------------------------------------------------------------------------
void ALPanelCineCamParams::onClickResetMode()
{
    const S32 mode = gSavedSettings.getS32("CinematicCamMode");
    for (const ModeEntry& entry : modeTable())
    {
        if (entry.mMode != mode)
        {
            continue;
        }
        for (const std::string& name : entry.mSettings)
        {
            if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
            {
                ctrl->resetToDefault(true);
            }
        }
        break;
    }
}

void ALPanelCineCamParams::onClickResetAll()
{
    LLHandle<ALPanelCineCamParams> handle = getDerivedHandle<ALPanelCineCamParams>();
    LLNotificationsUtil::add("CinematicCamConfirmResetAll", LLSD(), LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineCamParams* self = handle.get())
            {
                self->resetAllCallback(notification, response);
            }
        });
}

bool ALPanelCineCamParams::resetAllCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    for (const std::string& name : sharedSettings())
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
        {
            ctrl->resetToDefault(true);
        }
    }
    for (const ModeEntry& entry : modeTable())
    {
        for (const std::string& name : entry.mSettings)
        {
            if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
            {
                ctrl->resetToDefault(true);
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// named global presets: one LLSD .xml per preset in the per-user settings
// dir, { "mode": current mode, "settings": { name -> value } } covering the
// shared settings and every mode's settings (a complete rig state)
// ---------------------------------------------------------------------------
//static
std::string ALPanelCineCamParams::presetsDir()
{
    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESET_SUBDIR);
    if (!gDirUtilp->fileExists(dir))
    {
        LLFile::mkdir(dir);
    }
    return dir;
}

//static
std::string ALPanelCineCamParams::presetPath(const std::string& name)
{
    // same reversible sanitization the graphics presets use
    return gDirUtilp->add(presetsDir(), LLURI::escape(name) + ".xml");
}

void ALPanelCineCamParams::refreshPresetList(const std::string& select_name)
{
    if (!mPresetCombo)
    {
        return;
    }
    mPresetCombo->clearRows();

    std::vector<std::string> names;
    {
        LLDirIterator dir_iter(presetsDir(), "*.xml");
        std::string file;
        while (dir_iter.next(file))
        {
            names.emplace_back(LLURI::unescape(gDirUtilp->getBaseFileName(file, /*strip_exten=*/true)));
        }
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        mPresetCombo->add(name);
    }

    if (!select_name.empty() && mPresetCombo->setSelectedByValue(select_name, true))
    {
        return;
    }
    mPresetCombo->setLabel(getString("no_preset_label"));
}

void ALPanelCineCamParams::onClickSavePreset()
{
    LLSD args;
    // preselect the current name so "tweak and re-save" is one click
    if (mPresetCombo && !mPresetCombo->getSelectedItemLabel().empty())
    {
        args["DESC"] = mPresetCombo->getSelectedItemLabel();
    }
    else
    {
        args["DESC"] = LLStringUtil::null;
    }
    LLHandle<ALPanelCineCamParams> handle = getDerivedHandle<ALPanelCineCamParams>();
    LLNotificationsUtil::add("CinematicCamSavePreset", args, LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineCamParams* self = handle.get())
            {
                self->savePresetCallback(notification, response);
            }
        });
}

bool ALPanelCineCamParams::savePresetCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    std::string name = response["message"].asString();
    LLStringUtil::trim(name);
    if (name.empty())
    {
        return false;
    }
    writePreset(name);
    refreshPresetList(name);
    return false;
}

void ALPanelCineCamParams::writePreset(const std::string& name)
{
    LLSD settings = LLSD::emptyMap();
    auto capture = [&settings](const std::string& setting)
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
        {
            settings[setting] = ctrl->getValue();
        }
    };
    for (const std::string& setting : sharedSettings())
    {
        capture(setting);
    }
    for (const ModeEntry& entry : modeTable())
    {
        for (const std::string& setting : entry.mSettings)
        {
            capture(setting);
        }
    }

    LLSD preset = LLSD::emptyMap();
    preset["mode"] = gSavedSettings.getS32("CinematicCamMode");
    preset["settings"] = settings;

    const std::string path = presetPath(name);
    llofstream out(path.c_str());
    if (!out.is_open())
    {
        LL_WARNS("CinematicCam") << "Cannot write preset file " << path << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(preset, out);
    out.close();
    LL_INFOS("CinematicCam") << "Saved cinematic camera preset '" << name << "'" << LL_ENDL;
}

void ALPanelCineCamParams::onPresetSelected()
{
    if (mPresetCombo)
    {
        const std::string name = mPresetCombo->getSelectedItemLabel();
        if (!name.empty())
        {
            applyPreset(name);
        }
    }
}

void ALPanelCineCamParams::applyPreset(const std::string& name)
{
    const std::string path = presetPath(name);
    llifstream in(path.c_str());
    if (!in.is_open())
    {
        LL_WARNS("CinematicCam") << "Cannot open preset file " << path << LL_ENDL;
        return;
    }
    LLSD preset;
    LLSDSerialize::fromXML(preset, in);
    in.close();
    if (!preset.isMap() || !preset["settings"].isMap())
    {
        LL_WARNS("CinematicCam") << "Malformed preset file " << path << LL_ENDL;
        return;
    }

    // only apply keys this panel owns: a preset file is data, not commands
    auto apply = [&preset](const std::string& setting)
    {
        if (preset["settings"].has(setting))
        {
            if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
            {
                ctrl->setValue(preset["settings"][setting]);
            }
        }
    };
    for (const ModeEntry& entry : modeTable())
    {
        for (const std::string& setting : entry.mSettings)
        {
            apply(setting);
        }
    }
    for (const std::string& setting : sharedSettings())
    {
        apply(setting);
    }
    // the explicit mode key wins over any CinematicCamMode inside "settings"
    if (preset["mode"].isInteger())
    {
        gSavedSettings.setS32("CinematicCamMode", preset["mode"].asInteger());
    }
    LL_INFOS("CinematicCam") << "Applied cinematic camera preset '" << name << "'" << LL_ENDL;
}

void ALPanelCineCamParams::onClickDeletePreset()
{
    if (!mPresetCombo)
    {
        return;
    }
    const std::string name = mPresetCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLSD args;
    args["NAME"] = name;
    LLHandle<ALPanelCineCamParams> handle = getDerivedHandle<ALPanelCineCamParams>();
    LLNotificationsUtil::add("CinematicCamConfirmDeletePreset", args, LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineCamParams* self = handle.get())
            {
                self->deletePresetCallback(notification, response, name);
            }
        });
}

bool ALPanelCineCamParams::deletePresetCallback(const LLSD& notification, const LLSD& response,
                                                const std::string name)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    const std::string path = presetPath(name);
    if (LLFile::remove(path) != 0)
    {
        LL_WARNS("CinematicCam") << "Cannot delete preset file " << path << LL_ENDL;
    }
    refreshPresetList();
    return false;
}
