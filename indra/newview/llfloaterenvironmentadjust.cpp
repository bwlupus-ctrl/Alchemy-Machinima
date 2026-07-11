/**
 * @file llfloaterfixedenvironment.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterenvironmentadjust.h"

#include "llnotificationsutil.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llcolorswatch.h"
#include "lltexturectrl.h"
#include "llvirtualtrackball.h"
#include "llenvironment.h"
#include "llviewercontrol.h"
#include "pipeline.h"

// [BDMerge B13] BD - Windlight Stuff
#include "bdmergeenvlibrary.h"
#include "llagent.h"
#include "llcombobox.h"
#include "llfilepicker.h"
#include "llflyoutcombobtn.h"
#include "llinventorymodel.h"
#include "lllocalbitmaps.h"
#include "llpanel.h"
#include "llsettingsvo.h"
#include "lltrans.h"
#include "llviewermenufile.h"

//=========================================================================
namespace
{
    const std::string FIELD_SKY_AMBIENT_LIGHT("ambient_light");
    const std::string FIELD_SKY_BLUE_HORIZON("blue_horizon");
    const std::string FIELD_SKY_BLUE_DENSITY("blue_density");
    const std::string FIELD_SKY_SUN_COLOR("sun_color");
    const std::string FIELD_SKY_CLOUD_COLOR("cloud_color");
    const std::string FIELD_SKY_HAZE_HORIZON("haze_horizon");
    const std::string FIELD_SKY_HAZE_DENSITY("haze_density");
    const std::string FIELD_SKY_CLOUD_COVERAGE("cloud_coverage");
    const std::string FIELD_SKY_CLOUD_MAP("cloud_map");
    const std::string FIELD_WATER_NORMAL_MAP("water_normal_map");
    const std::string FIELD_SKY_CLOUD_SCALE("cloud_scale");
    const std::string FIELD_SKY_SCENE_GAMMA("scene_gamma");
    const std::string FIELD_SKY_SUN_ROTATION("sun_rotation");
    const std::string FIELD_SKY_SUN_AZIMUTH("sun_azimuth");
    const std::string FIELD_SKY_SUN_ELEVATION("sun_elevation");
    const std::string FIELD_SKY_SUN_SCALE("sun_scale");
    const std::string FIELD_SKY_GLOW_FOCUS("glow_focus");
    const std::string FIELD_SKY_GLOW_SIZE("glow_size");
    const std::string FIELD_SKY_STAR_BRIGHTNESS("star_brightness");
    const std::string FIELD_SKY_MOON_ROTATION("moon_rotation");
    const std::string FIELD_SKY_MOON_AZIMUTH("moon_azimuth");
    const std::string FIELD_SKY_MOON_ELEVATION("moon_elevation");
    const std::string FIELD_REFLECTION_PROBE_AMBIANCE("probe_ambiance");
    const std::string BTN_RESET("btn_reset");

    // [BDMerge B13] BD - Windlight Stuff
    const std::string ACTION_SAVELOCAL("save_as_local_setting");
    const std::string ACTION_SAVEAS("save_as_new_settings");
    const std::string FIELD_SKY_CLOUD_LOCK_X("cloud_lock_x");
    const std::string FIELD_SKY_CLOUD_LOCK_Y("cloud_lock_y");
    const std::string BTN_SAVE("save");
    const std::string BTN_DELETE("delete");
    const std::string BTN_IMPORT("import");
    const std::string EDITOR_NAME("sky_preset_combo");
    const std::string BUTTON_NAME_FLYOUT("btn_flyout");
    const std::string PANEL_BDMERGE("lp_bdmerge");
    const std::string XML_FLYOUTMENU_FILE("menu_save_settings_adjust.xml");

    // [BDMerge B13] gate; off = stock behavior
    bool bdmerge_env_gate()
    {
        static LLCachedControl<bool> gate(gSavedSettings, "BDMergeEnvLocalPresets", false);
        return gate;
    }

    const F32 SLIDER_SCALE_SUN_AMBIENT(3.0f);
    const F32 SLIDER_SCALE_BLUE_HORIZON_DENSITY(2.0f);
    const F32 SLIDER_SCALE_GLOW_R(20.0f);
    const F32 SLIDER_SCALE_GLOW_B(-5.0f);
    //const F32 SLIDER_SCALE_DENSITY_MULTIPLIER(0.001f);

    const S32 FLOATER_ENVIRONMENT_UPDATE(-2);
}

//=========================================================================
LLFloaterEnvironmentAdjust::LLFloaterEnvironmentAdjust(const LLSD &key):
    LLFloater(key)
{}

LLFloaterEnvironmentAdjust::~LLFloaterEnvironmentAdjust()
{
    // [BDMerge B13]
    delete mFlyoutControl;
}

//-------------------------------------------------------------------------
bool LLFloaterEnvironmentAdjust::postBuild()
{
    getChild<LLUICtrl>(FIELD_SKY_AMBIENT_LIGHT)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onAmbientLightChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_BLUE_HORIZON)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onBlueHorizonChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_BLUE_DENSITY)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onBlueDensityChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_HAZE_HORIZON)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onHazeHorizonChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_HAZE_DENSITY)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onHazeDensityChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSceneGammaChanged(); });

    getChild<LLUICtrl>(FIELD_SKY_CLOUD_COLOR)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudColorChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_COVERAGE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudCoverageChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCALE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudScaleChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_COLOR)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunColorChanged(); });

    getChild<LLUICtrl>(FIELD_SKY_GLOW_FOCUS)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onGlowChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_GLOW_SIZE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onGlowChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_STAR_BRIGHTNESS)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onStarBrightnessChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_ROTATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunRotationChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunAzimElevChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunAzimElevChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_SCALE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunScaleChanged(); });

    getChild<LLUICtrl>(FIELD_SKY_MOON_ROTATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonRotationChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonAzimElevChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonAzimElevChanged(); });
    getChild<LLUICtrl>(BTN_RESET)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onButtonReset(); });

    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudMapChanged(); });
    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setDefaultImageAssetID(LLSettingsSky::GetDefaultCloudNoiseTextureId());
    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setAllowNoTexture(true);

    getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setDefaultImageAssetID(LLSettingsWater::GetDefaultWaterNormalAssetId());
    getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setBlankImageAssetID(BLANK_OBJECT_NORMAL);
    getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onWaterMapChanged(); });

    getChild<LLUICtrl>(FIELD_REFLECTION_PROBE_AMBIANCE)->setCommitCallback([this](LLUICtrl*, const LLSD&) { onReflectionProbeAmbianceChanged(); });

    // [BDMerge B13] BD - Windlight Stuff: preset combo, save/delete/import,
    // cloud scroll locks. All the widgets live in a hidden bottom strip
    // (lp_bdmerge, visible="false"); when the gate is off nothing below is
    // shown or wired, so stock appearance/behavior is unchanged.
    if (bdmerge_env_gate())
    {
        if (LLPanel* bd_panel = findChild<LLPanel>(PANEL_BDMERGE))
        {
            bd_panel->setVisible(true);
            // grow the floater by the strip's height so the stock controls
            // keep their full size
            reshape(getRect().getWidth(), getRect().getHeight() + bd_panel->getRect().getHeight());

            mCloudScrollLockX = getChild<LLUICtrl>(FIELD_SKY_CLOUD_LOCK_X);
            mCloudScrollLockX->setCommitCallback([this](LLUICtrl *ctrl, const LLSD &) { onCloudScrollXLocked(ctrl->getValue()); });
            mCloudScrollLockY = getChild<LLUICtrl>(FIELD_SKY_CLOUD_LOCK_Y);
            mCloudScrollLockY->setCommitCallback([this](LLUICtrl *ctrl, const LLSD &) { onCloudScrollYLocked(ctrl->getValue()); });

            getChild<LLUICtrl>(BTN_DELETE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onButtonDelete(); });
            getChild<LLUICtrl>(BTN_IMPORT)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onButtonImport(); });

            mFlyoutControl = new LLFlyoutComboBtnCtrl(this, BTN_SAVE, BUTTON_NAME_FLYOUT, XML_FLYOUTMENU_FILE, false);
            mFlyoutControl->setAction([this](LLUICtrl *ctrl, const LLSD &data) { onButtonApply(ctrl, data); });

            mNameCombo = getChild<LLComboBox>(EDITOR_NAME);
            mNameCombo->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSelectPreset(); });
        }
    }

    refresh();
    return true;
}

void LLFloaterEnvironmentAdjust::onOpen(const LLSD& key)
{
    if (!mLiveSky)
    {
        LLEnvironment::instance().saveBeaconsState();
    }
    captureCurrentEnvironment();

    mEventConnection = LLEnvironment::instance().setEnvironmentChanged([this](LLEnvironment::EnvSelection_t env, S32 version){ onEnvironmentUpdated(env, version); });

    // HACK -- resume reflection map manager because "setEnvironmentChanged" may pause it (SL-20456)
    gPipeline.mReflectionMapManager.resume();

    LLFloater::onOpen(key);
    refresh();

    // [BDMerge B13] BD - Windlight Stuff: populate the sky preset combo
    if (bdmerge_env_gate() && mNameCombo)
    {
        gBDMergeEnvLibrary.loadPresetsFromDir(mNameCombo, "skies");
        gBDMergeEnvLibrary.addInventoryPresets(mNameCombo, mLiveSky);
    }
}

void LLFloaterEnvironmentAdjust::onClose(bool app_quitting)
{
    LLEnvironment::instance().revertBeaconsState();
    mEventConnection.disconnect();
    mLiveSky.reset();
    mLiveWater.reset();
    LLFloater::onClose(app_quitting);
}


//-------------------------------------------------------------------------
void LLFloaterEnvironmentAdjust::refresh()
{
    if (!mLiveSky || !mLiveWater)
    {
        setAllChildrenEnabled(false);
        return;
    }

    setEnabled(true);
    setAllChildrenEnabled(true);

    getChild<LLColorSwatchCtrl>(FIELD_SKY_AMBIENT_LIGHT)->set(mLiveSky->getAmbientColor() / SLIDER_SCALE_SUN_AMBIENT);
    getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_HORIZON)->set(mLiveSky->getBlueHorizon() / SLIDER_SCALE_BLUE_HORIZON_DENSITY);
    getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_DENSITY)->set(mLiveSky->getBlueDensity() / SLIDER_SCALE_BLUE_HORIZON_DENSITY);
    getChild<LLUICtrl>(FIELD_SKY_HAZE_HORIZON)->setValue(mLiveSky->getHazeHorizon());
    getChild<LLUICtrl>(FIELD_SKY_HAZE_DENSITY)->setValue(mLiveSky->getHazeDensity());
    getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setValue(mLiveSky->getGamma());
    getChild<LLColorSwatchCtrl>(FIELD_SKY_CLOUD_COLOR)->set(mLiveSky->getCloudColor());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_COVERAGE)->setValue(mLiveSky->getCloudShadow());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCALE)->setValue(mLiveSky->getCloudScale());
    getChild<LLColorSwatchCtrl>(FIELD_SKY_SUN_COLOR)->set(mLiveSky->getSunlightColor() / SLIDER_SCALE_SUN_AMBIENT);

    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setValue(mLiveSky->getCloudNoiseTextureId());
    getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setValue(mLiveWater->getNormalMapID());

    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    getChild<LLUICtrl>(FIELD_REFLECTION_PROBE_AMBIANCE)->setValue(mLiveSky->getReflectionProbeAmbiance(should_auto_adjust));

    LLColor3 glow(mLiveSky->getGlow());

    // takes 40 - 0.2 range -> 0 - 1.99 UI range
    getChild<LLUICtrl>(FIELD_SKY_GLOW_SIZE)->setValue(2.0 - (glow.mV[0] / SLIDER_SCALE_GLOW_R));
    getChild<LLUICtrl>(FIELD_SKY_GLOW_FOCUS)->setValue(glow.mV[2] / SLIDER_SCALE_GLOW_B);
    getChild<LLUICtrl>(FIELD_SKY_STAR_BRIGHTNESS)->setValue(mLiveSky->getStarBrightness());
    getChild<LLUICtrl>(FIELD_SKY_SUN_SCALE)->setValue(mLiveSky->getSunScale());

    // Sun rotation
    LLQuaternion quat = mLiveSky->getSunRotation();
    F32 azimuth;
    F32 elevation;
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);

    getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->setValue(azimuth);
    getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->setValue(elevation);
    getChild<LLVirtualTrackball>(FIELD_SKY_SUN_ROTATION)->setRotation(quat);

    // Moon rotation
    quat = mLiveSky->getMoonRotation();
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);

    getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->setValue(azimuth);
    getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->setValue(elevation);
    getChild<LLVirtualTrackball>(FIELD_SKY_MOON_ROTATION)->setRotation(quat);

    // [BDMerge B13] BD - Windlight Stuff: reflect the env-wide lock state
    if (mCloudScrollLockX && mCloudScrollLockY)
    {
        LLEnvironment &environment(LLEnvironment::instance());
        mCloudScrollLockX->setValue(environment.isCloudScrollXLocked());
        mCloudScrollLockY->setValue(environment.isCloudScrollYLocked());
    }

    updateGammaLabel();
}


void LLFloaterEnvironmentAdjust::captureCurrentEnvironment()
{
    LLEnvironment &environment(LLEnvironment::instance());
    bool updatelocal(false);

    if (environment.hasEnvironment(LLEnvironment::ENV_LOCAL))
    {
        if (environment.getEnvironmentDay(LLEnvironment::ENV_LOCAL))
        {   // We have a full day cycle in the local environment.  Freeze the sky
            mLiveSky = environment.getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL)->buildClone();
            mLiveWater = environment.getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL)->buildClone();
            updatelocal = true;
        }
        else
        {   // otherwise we can just use the sky.
            mLiveSky = environment.getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL);
            mLiveWater = environment.getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL);
        }
    }
    else
    {
        mLiveSky = environment.getEnvironmentFixedSky(LLEnvironment::ENV_PARCEL, true)->buildClone();
        mLiveWater = environment.getEnvironmentFixedWater(LLEnvironment::ENV_PARCEL, true)->buildClone();
        updatelocal = true;
    }

    if (updatelocal)
    {
        environment.setEnvironment(LLEnvironment::ENV_LOCAL, mLiveSky, FLOATER_ENVIRONMENT_UPDATE);
        environment.setEnvironment(LLEnvironment::ENV_LOCAL, mLiveWater, FLOATER_ENVIRONMENT_UPDATE);
    }
    environment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL, LLEnvironment::TRANSITION_INSTANT);
}

void LLFloaterEnvironmentAdjust::onButtonReset()
{
    LLNotificationsUtil::add("PersonalSettingsConfirmReset", LLSD(), LLSD(),
        [this](const LLSD&notif, const LLSD&resp)
    {
        S32 opt = LLNotificationsUtil::getSelectedOption(notif, resp);
        if (opt == 0)
        {
            this->closeFloater();
            LLEnvironment::instance().clearEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        }
    });

}
//-------------------------------------------------------------------------
void LLFloaterEnvironmentAdjust::onAmbientLightChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setAmbientColor(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_AMBIENT_LIGHT)->get() * SLIDER_SCALE_SUN_AMBIENT));
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onBlueHorizonChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setBlueHorizon(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_HORIZON)->get() * SLIDER_SCALE_BLUE_HORIZON_DENSITY));
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onBlueDensityChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setBlueDensity(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_DENSITY)->get() * SLIDER_SCALE_BLUE_HORIZON_DENSITY));
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onHazeHorizonChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setHazeHorizon((F32)getChild<LLUICtrl>(FIELD_SKY_HAZE_HORIZON)->getValue().asReal());
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onHazeDensityChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setHazeDensity((F32)getChild<LLUICtrl>(FIELD_SKY_HAZE_DENSITY)->getValue().asReal());
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onSceneGammaChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setGamma((F32)getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->getValue().asReal());
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onCloudColorChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setCloudColor(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_CLOUD_COLOR)->get()));
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onCloudCoverageChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setCloudShadow((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_COVERAGE)->getValue().asReal());
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onCloudScaleChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setCloudScale((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCALE)->getValue().asReal());
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onGlowChanged()
{
    if (!mLiveSky)
        return;
    LLColor3 glow((F32)getChild<LLUICtrl>(FIELD_SKY_GLOW_SIZE)->getValue().asReal(), 0.0f, (F32)getChild<LLUICtrl>(FIELD_SKY_GLOW_FOCUS)->getValue().asReal());

    // takes 0 - 1.99 UI range -> 40 -> 0.2 range
    glow.mV[0] = (2.0f - glow.mV[0]) * SLIDER_SCALE_GLOW_R;
    glow.mV[2] *= SLIDER_SCALE_GLOW_B;

    mLiveSky->setGlow(glow);
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onStarBrightnessChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setStarBrightness((F32)getChild<LLUICtrl>(FIELD_SKY_STAR_BRIGHTNESS)->getValue().asReal());
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onSunRotationChanged()
{
    LLQuaternion quat = getChild<LLVirtualTrackball>(FIELD_SKY_SUN_ROTATION)->getRotation();
    F32 azimuth;
    F32 elevation;
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);
    getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->setValue(azimuth);
    getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->setValue(elevation);
    if (mLiveSky)
    {
        mLiveSky->setSunRotation(quat);
        mLiveSky->update();
    }
}

void LLFloaterEnvironmentAdjust::onSunAzimElevChanged()
{
    F32 azimuth = (F32)getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->getValue().asReal();
    F32 elevation = (F32)getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->getValue().asReal();
    LLQuaternion quat;

    azimuth *= DEG_TO_RAD;
    elevation *= DEG_TO_RAD;

    if (is_approx_zero(elevation))
    {
        elevation = F_APPROXIMATELY_ZERO;
    }

    quat.setAngleAxis(-elevation, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - azimuth, 0, 0, 1);
    quat *= az_quat;

    getChild<LLVirtualTrackball>(FIELD_SKY_SUN_ROTATION)->setRotation(quat);

    if (mLiveSky)
    {
        mLiveSky->setSunRotation(quat);
        mLiveSky->update();
    }
}

void LLFloaterEnvironmentAdjust::onSunScaleChanged()
{
    if (!mLiveSky)
        return;
    mLiveSky->setSunScale((F32)(getChild<LLUICtrl>(FIELD_SKY_SUN_SCALE)->getValue().asReal()));
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onMoonRotationChanged()
{
    LLQuaternion quat = getChild<LLVirtualTrackball>(FIELD_SKY_MOON_ROTATION)->getRotation();
    F32 azimuth;
    F32 elevation;
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);
    getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->setValue(azimuth);
    getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->setValue(elevation);
    if (mLiveSky)
    {
        mLiveSky->setMoonRotation(quat);
        mLiveSky->update();
    }
}

void LLFloaterEnvironmentAdjust::onMoonAzimElevChanged()
{
    F32 azimuth = (F32)getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->getValue().asReal();
    F32 elevation = (F32)getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->getValue().asReal();
    LLQuaternion quat;

    azimuth *= DEG_TO_RAD;
    elevation *= DEG_TO_RAD;

    if (is_approx_zero(elevation))
    {
        elevation = F_APPROXIMATELY_ZERO;
    }

    quat.setAngleAxis(-elevation, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - azimuth, 0, 0, 1);
    quat *= az_quat;

    getChild<LLVirtualTrackball>(FIELD_SKY_MOON_ROTATION)->setRotation(quat);

    if (mLiveSky)
    {
        mLiveSky->setMoonRotation(quat);
        mLiveSky->update();
    }
}

void LLFloaterEnvironmentAdjust::onCloudMapChanged()
{
    if (!mLiveSky)
    {
        return;
    }

    LLTextureCtrl* picker_ctrl = getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP);

    LLUUID new_texture_id = picker_ctrl->getValue().asUUID();

    LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);

    LLSettingsSky::ptr_t sky_to_set = mLiveSky->buildClone();
    if (!sky_to_set)
    {
        return;
    }

    sky_to_set->setCloudNoiseTextureId(new_texture_id);

    LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, sky_to_set);

    LLEnvironment::instance().updateEnvironment(LLEnvironment::TRANSITION_INSTANT, true);

    picker_ctrl->setValue(new_texture_id);
}

void LLFloaterEnvironmentAdjust::onWaterMapChanged()
{
    if (!mLiveWater)
        return;
    mLiveWater->setNormalMapID(getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->getValue().asUUID());
    mLiveWater->update();
}

void LLFloaterEnvironmentAdjust::onSunColorChanged()
{
    if (!mLiveSky)
        return;
    LLColor3 color(getChild<LLColorSwatchCtrl>(FIELD_SKY_SUN_COLOR)->get());

    color *= SLIDER_SCALE_SUN_AMBIENT;

    mLiveSky->setSunlightColor(color);
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::onReflectionProbeAmbianceChanged()
{
    if (!mLiveSky) return;
    F32 ambiance = (F32)getChild<LLUICtrl>(FIELD_REFLECTION_PROBE_AMBIANCE)->getValue().asReal();
    mLiveSky->setReflectionProbeAmbiance(ambiance);

    updateGammaLabel();
    mLiveSky->update();
}

void LLFloaterEnvironmentAdjust::updateGammaLabel()
{
    if (!mLiveSky) return;

    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    F32 ambiance = mLiveSky->getReflectionProbeAmbiance(should_auto_adjust);
    if (ambiance != 0.f)
    {
        childSetValue("scene_gamma_label", getString("hdr_string"));
        getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setToolTip(getString("hdr_tooltip"));
    }
    else
    {
        childSetValue("scene_gamma_label", getString("brightness_string"));
        getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setToolTip(std::string());
    }
}

void LLFloaterEnvironmentAdjust::onEnvironmentUpdated(LLEnvironment::EnvSelection_t env, S32 version)
{
    if (env == LLEnvironment::ENV_LOCAL)
    {   // a new local environment has been applied
        if (version != FLOATER_ENVIRONMENT_UPDATE)
        {   // not by this floater
            captureCurrentEnvironment();
            refresh();
        }
    }
}

// [BDMerge B13] BD - Windlight Stuff (donor: BD llfloaterenvironmentadjust.cpp
// lines 753-937). Only reachable when BDMergeEnvLocalPresets is enabled
// (controls hidden and unwired otherwise).
//=====================================================================================================
void LLFloaterEnvironmentAdjust::onCloudScrollXLocked(bool lock)
{
    if (!mLiveSky)
        return;
    LLEnvironment::instance().pauseCloudScrollX(lock);
    refresh();
}

void LLFloaterEnvironmentAdjust::onCloudScrollYLocked(bool lock)
{
    if (!mLiveSky)
        return;
    LLEnvironment::instance().pauseCloudScrollY(lock);
    refresh();
}

void LLFloaterEnvironmentAdjust::onButtonApply(LLUICtrl *ctrl, const LLSD &data)
{
    std::string ctrl_action = ctrl->getName();

    std::string local_desc;
    LLSettingsBase::ptr_t setting_clone;
    bool is_local = false; // because getString can be empty
    if (mLiveSky)
    {
        setting_clone = mLiveSky->buildClone();
        if (LLLocalBitmapMgr::getInstance()->isLocal(mLiveSky->getSunTextureId()))
        {
            local_desc = LLTrans::getString("EnvironmentSun");
            is_local = true;
        }
        else if (LLLocalBitmapMgr::getInstance()->isLocal(mLiveSky->getMoonTextureId()))
        {
            local_desc = LLTrans::getString("EnvironmentMoon");
            is_local = true;
        }
        else if (LLLocalBitmapMgr::getInstance()->isLocal(mLiveSky->getCloudNoiseTextureId()))
        {
            local_desc = LLTrans::getString("EnvironmentCloudNoise");
            is_local = true;
        }
        else if (LLLocalBitmapMgr::getInstance()->isLocal(mLiveSky->getBloomTextureId()))
        {
            local_desc = LLTrans::getString("EnvironmentBloom");
            is_local = true;
        }
    }

    if (is_local)
    {
        LLSD args;
        args["FIELD"] = local_desc;
        LLNotificationsUtil::add("WLLocalTextureFixedBlock", args);
        return;
    }

    if (ctrl_action == ACTION_SAVELOCAL)
    {
        onButtonSave();
    }
    else if (ctrl_action == ACTION_SAVEAS)
    {
        LLSD args;
        args["DESC"] = mLiveSky->getName();
        LLNotificationsUtil::add("SaveSettingAs", args, LLSD(), boost::bind(&LLFloaterEnvironmentAdjust::onSaveAsCommit, this, _1, _2, setting_clone));
    }
    else
    {
        LL_WARNS("ENVIRONMENT") << "Unknown settings action '" << ctrl_action << "'" << LL_ENDL;
    }
}

void LLFloaterEnvironmentAdjust::onSaveAsCommit(const LLSD& notification, const LLSD& response, const LLSettingsBase::ptr_t &settings)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (0 == option)
    {
        std::string settings_name = response["message"].asString();

        LLInventoryObject::correctInventoryName(settings_name);
        if (settings_name.empty())
        {
            // Ideally notification should disable 'OK' button if name won't fit our requirements,
            // for now either display notification, or use some default name
            settings_name = "Unnamed";
        }

        doApplyCreateNewInventory(settings_name, settings);
    }
}

void LLFloaterEnvironmentAdjust::doApplyCreateNewInventory(std::string settings_name, const LLSettingsBase::ptr_t &settings)
{
    LLUUID parent_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
    // This method knows what sort of settings object to create.
    LLSettingsVOBase::createInventoryItem(settings, parent_id, settings_name,
        [this](LLUUID asset_id, LLUUID inventory_id, LLUUID, LLSD results) { onInventoryCreated(asset_id, inventory_id, results); });
}

void LLFloaterEnvironmentAdjust::onInventoryCreated(LLUUID asset_id, LLUUID inventory_id, LLSD results)
{
    LL_WARNS("ENVIRONMENT") << "Inventory item " << inventory_id << " has been created with asset " << asset_id << " results are:" << results << LL_ENDL;

    if (inventory_id.isNull() || !results["success"].asBoolean())
    {
        LLNotificationsUtil::add("CantCreateInventory");
        return;
    }
}

void LLFloaterEnvironmentAdjust::onButtonSave()
{
    if (!mLiveSky || !mNameCombo) return;

    LLSettingsSky::ptr_t sky = mLiveSky->buildClone();

    gBDMergeEnvLibrary.savePreset(mNameCombo->getValue(), sky);
    gBDMergeEnvLibrary.loadPresetsFromDir(mNameCombo, "skies");
    gBDMergeEnvLibrary.addInventoryPresets(mNameCombo, sky);
}

void LLFloaterEnvironmentAdjust::onButtonDelete()
{
    if (!mLiveSky || !mNameCombo) return;
    gBDMergeEnvLibrary.deletePreset(mNameCombo->getValue(), "skies");
    gBDMergeEnvLibrary.loadPresetsFromDir(mNameCombo, "skies");
    gBDMergeEnvLibrary.addInventoryPresets(mNameCombo, mLiveSky);
}

void LLFloaterEnvironmentAdjust::onButtonImport()
{   // Load a legacy Windlight XML from disk.
    LLFilePickerReplyThread::startPicker(boost::bind(&LLFloaterEnvironmentAdjust::loadSkySettingFromFile, this, _1), LLFilePicker::FFLOAD_XML, false);
}

void LLFloaterEnvironmentAdjust::loadSkySettingFromFile(const std::vector<std::string>& filenames)
{
    if (!mLiveSky) return;
    if (filenames.size() < 1) return;
    std::string filename = filenames[0];
    gBDMergeEnvLibrary.loadPreset(filename, mLiveSky);
    refresh();
}

void LLFloaterEnvironmentAdjust::onSelectPreset()
{
    if (!mLiveSky || !mNameCombo) return;
    gBDMergeEnvLibrary.onSelectPreset(mNameCombo, mLiveSky);
    refresh();
}
