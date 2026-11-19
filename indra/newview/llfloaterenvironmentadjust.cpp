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
#include "llbutton.h" // [EnvIntensity v2] Advanced toggle label
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
#include "llfloaterreg.h"       // [EnvIntensity userpresets] Presets... button
#include "llflyoutcombobtn.h"
#include "alenvintensitypresets.h" // [EnvIntensity userpresets]
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

    // [EnvIntensity v2] Advanced exposure panel (lp_env_intensity_adv). Height
    // mirrors the XML; the outer layout stack charges its 3 px panel spacing
    // for every visible panel that is not the literal last one, so showing the
    // panel costs height + spacing (335 -> 448 collapsed -> expanded).
    const std::string PANEL_ENV_INTENSITY_ADV("lp_env_intensity_adv");
    const S32 ENV_INTENSITY_ADV_PANEL_HEIGHT(110);
    const S32 ENV_INTENSITY_PANEL_SPACING(3);

    // [EnvIntensity v2] every Advanced-panel key: "Reset advanced" restores
    // these (never the two masters AlchemyEnvSunEV / AlchemyEnvSkyGIEV, never
    // the panel-state key) and then explicitly re-links the moon.
    const char* const ENV_INTENSITY_ADV_KEYS[] =
    {
        "AlchemyEnvMoonEV",
        "AlchemyEnvGIAmbientEV",
        "AlchemyEnvGIProbeDiffuseEV",
        "AlchemyEnvGIProbeSpecEV",
        "AlchemyEnvLocalLightEV",
        "AlchemyEnvLocalLightIncludeRig",
        "AlchemyEnvSunKelvin",
        "AlchemyEnvSunTintColor",
        "AlchemyEnvSunTintStrength",
        "AlchemyEnvMoonTintColor",
        "AlchemyEnvMoonTintStrength",
        "AlchemyEnvAmbientTintColor",
        "AlchemyEnvAmbientTintStrength",
        "AlchemyEnvShadowLiftEV",
    };

    // [EnvIntensity v2] per-cell reset buttons -> setting(s) restored to default
    struct EnvIntensityResetBinding
    {
        const char* mButton;
        const char* mSetting;
        const char* mSetting2; // tint cells reset swatch + strength together
    };
    const EnvIntensityResetBinding ENV_INTENSITY_RESETS[] =
    {
        { "env_moon_ev_reset",       "AlchemyEnvMoonEV",           nullptr },
        { "env_local_ev_reset",      "AlchemyEnvLocalLightEV",     nullptr },
        { "env_probe_diff_ev_reset", "AlchemyEnvGIProbeDiffuseEV", nullptr },
        { "env_probe_spec_ev_reset", "AlchemyEnvGIProbeSpecEV",    nullptr },
        { "env_amb_ev_reset",        "AlchemyEnvGIAmbientEV",      nullptr },
        { "env_shadow_lift_reset",   "AlchemyEnvShadowLiftEV",     nullptr },
        { "env_sun_kelvin_reset",    "AlchemyEnvSunKelvin",        nullptr },
        { "env_sun_tint_reset",      "AlchemyEnvSunTintColor",     "AlchemyEnvSunTintStrength" },
        { "env_moon_tint_reset",     "AlchemyEnvMoonTintColor",    "AlchemyEnvMoonTintStrength" },
        { "env_amb_tint_reset",      "AlchemyEnvAmbientTintColor", "AlchemyEnvAmbientTintStrength" },
    };

    void env_intensity_reset_setting(const std::string& name)
    {
        if (auto ctrl = gSavedSettings.getControl(name))
        {
            ctrl->resetToDefault(true);
        }
    }

    // [EnvIntensity presets] "Quick Presets" combo (env_intensity_preset) on the
    // Light Intensity strip. Every row writes the FULL set of fields below
    // (unlisted in the design -> the Stock/default value, taken from
    // settings_alchemy.xml), so applying a preset is deterministic no matter
    // what was set before. Never includes AlchemyEnvIntensityAdvanced,
    // AlchemyEnvLocalLightIncludeRig or RenderDynamicExposureEnabled (auto
    // exposure): those stay exactly as the user left them.
    struct EnvIntensityPreset
    {
        const char* mKey; // combo_box.item value; matches an XUI entry
        F32         mSunEV;
        F32         mSkyGIEV;
        bool        mMoonLinked;
        F32         mMoonEV;
        F32         mGIAmbientEV;
        F32         mGIProbeDiffuseEV;
        F32         mGIProbeSpecEV;
        F32         mLocalLightEV;
        F32         mSunKelvin;
        LLColor4    mSunTintColor;
        F32         mSunTintStrength;
        LLColor4    mMoonTintColor;
        F32         mMoonTintStrength;
        LLColor4    mAmbientTintColor;
        F32         mAmbientTintStrength;
        F32         mShadowLiftEV;
    };

    const LLColor4 ENV_TINT_WHITE(1.f, 1.f, 1.f, 1.f);

    const EnvIntensityPreset ENV_INTENSITY_PRESETS[] =
    {
        // key                    sunEV  skyGIEV moonLinked moonEV giAmbEV giDiffEV giSpecEV localEV kelvin   sunTint                             sunStr moonTint                            moonStr ambTint                             ambStr liftEV
        { "stock",                 0.00f, 0.00f, true,       0.00f, 0.00f,  0.00f,   0.00f,   0.00f, 6500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 0.00f },
        { "golden_hour",           0.50f, 0.00f, true,       0.00f, 0.00f,  0.00f,   0.00f,   0.00f, 3600.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, LLColor4(1.00f, 0.85f, 0.70f, 1.f), 0.35f, 0.25f },
        { "blue_hour",            -1.00f, 0.30f, true,       0.00f, 0.00f,  0.00f,   0.00f,   0.00f, 9000.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, LLColor4(0.70f, 0.80f, 1.00f, 1.f), 0.50f, 0.30f },
        { "overcast_soft",        -1.00f, 0.70f, true,       0.00f, 0.00f,  0.00f,  -0.30f,   0.00f, 7000.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 0.60f },
        { "noon_punch",            1.00f,-0.30f, true,       0.00f, 0.00f,  0.00f,   0.30f,   0.00f, 5800.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 0.00f },
        { "high_key_studio",       0.30f, 0.80f, true,       0.00f, 0.00f,  0.00f,   0.00f,   0.00f, 6500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 1.00f },
        { "low_key_noir",          0.50f,-1.50f, true,       0.00f,-0.50f,  0.00f,   0.50f,   0.50f, 5000.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 0.00f },
        { "moonlit_night",         0.00f,-0.50f, false,      1.50f, 0.00f,  0.00f,   0.00f,   0.00f, 6500.f, ENV_TINT_WHITE,                     1.00f, LLColor4(0.75f, 0.85f, 1.00f, 1.f),0.60f, ENV_TINT_WHITE,                     1.00f, 0.20f },
        { "neon_night",           -1.00f,-1.00f, true,       0.00f, 0.00f,  0.00f,   0.70f,   1.50f, 6500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, LLColor4(0.55f, 0.80f, 1.00f, 1.f), 0.40f, 0.20f },
        { "candlelit_interior",   -2.00f,-1.00f, true,       0.00f, 0.00f,  0.00f,   0.00f,   2.00f, 6500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, LLColor4(1.00f, 0.75f, 0.50f, 1.f), 0.40f, 0.30f },
        { "desert_heat",           1.20f, 0.00f, true,       0.00f, 0.00f,  0.00f,   0.20f,   0.00f, 5200.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, LLColor4(1.00f, 0.90f, 0.75f, 1.f), 0.30f, 0.40f },
        { "arctic_cold",           0.50f, 0.30f, true,       0.00f, 0.00f,  0.00f,   0.00f,   0.00f, 8500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, LLColor4(0.80f, 0.90f, 1.00f, 1.f), 0.40f, 0.40f },
        { "soft_fill",             0.00f, 0.00f, true,       0.00f, 0.00f,  0.00f,   0.00f,   0.00f, 6500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 0.80f },
        { "reflections_pop",       0.00f, 0.00f, true,       0.00f, 0.00f,  0.00f,   1.00f,   0.00f, 6500.f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, ENV_TINT_WHITE,                     1.00f, 0.00f },
    };

    // [EnvIntensity presets] Custom-detection: every setting a preset can
    // touch gets a signal listener (connected in postBuild) that calls
    // refreshEnvIntensityPresetCombo(). Cheap: only fires on an actual change,
    // never per frame.
    const char* const ENV_INTENSITY_PRESET_WATCHED_KEYS[] =
    {
        "AlchemyEnvSunEV",
        "AlchemyEnvSkyGIEV",
        "AlchemyEnvMoonLinked",
        "AlchemyEnvMoonEV",
        "AlchemyEnvGIAmbientEV",
        "AlchemyEnvGIProbeDiffuseEV",
        "AlchemyEnvGIProbeSpecEV",
        "AlchemyEnvLocalLightEV",
        "AlchemyEnvSunKelvin",
        "AlchemyEnvSunTintColor",
        "AlchemyEnvSunTintStrength",
        "AlchemyEnvMoonTintColor",
        "AlchemyEnvMoonTintStrength",
        "AlchemyEnvAmbientTintColor",
        "AlchemyEnvAmbientTintStrength",
        "AlchemyEnvShadowLiftEV",
    };

    // [EnvIntensity userpresets] Combo value prefix of the user-preset items
    // ("user:<name>"), and the section header item's value. The epsilon compare
    // moved to ALEnvIntensityPresets::nearlyEqual (same 1e-4 epsilon).
    const std::string ENV_INTENSITY_USER_PREFIX("user:");
    const std::string ENV_INTENSITY_USER_HEADER_VALUE("user_header");

    // [EnvIntensity userpresets] built-in table row -> the shared 16-value struct
    // (the table itself is untouched; the field order there is positional).
    ALEnvIntensityPresets::Values env_intensity_builtin_values(const EnvIntensityPreset& p)
    {
        ALEnvIntensityPresets::Values v;
        v.mSunEV               = p.mSunEV;
        v.mSkyGIEV             = p.mSkyGIEV;
        v.mMoonLinked          = p.mMoonLinked;
        v.mMoonEV              = p.mMoonEV;
        v.mGIAmbientEV         = p.mGIAmbientEV;
        v.mGIProbeDiffuseEV    = p.mGIProbeDiffuseEV;
        v.mGIProbeSpecEV       = p.mGIProbeSpecEV;
        v.mLocalLightEV        = p.mLocalLightEV;
        v.mSunKelvin           = p.mSunKelvin;
        v.mSunTintColor        = p.mSunTintColor;
        v.mSunTintStrength     = p.mSunTintStrength;
        v.mMoonTintColor       = p.mMoonTintColor;
        v.mMoonTintStrength    = p.mMoonTintStrength;
        v.mAmbientTintColor    = p.mAmbientTintColor;
        v.mAmbientTintStrength = p.mAmbientTintStrength;
        v.mShadowLiftEV        = p.mShadowLiftEV;
        return v;
    }
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

    // [EnvIntensity] Light Intensity strip (lp_env_intensity). The sliders and
    // the Auto exposure checkbox bind to viewer settings via control_name in the
    // XML; only the reset-to-0-EV buttons need code. Viewer-side multipliers,
    // never written into the sky asset (mLiveSky is untouched).
    if (LLUICtrl* sun_reset = findChild<LLUICtrl>("env_sun_ev_reset"))
    {
        sun_reset->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            if (auto ctrl = gSavedSettings.getControl("AlchemyEnvSunEV")) { ctrl->resetToDefault(true); }
        });
    }
    if (LLUICtrl* gi_reset = findChild<LLUICtrl>("env_gi_ev_reset"))
    {
        gi_reset->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            if (auto ctrl = gSavedSettings.getControl("AlchemyEnvSkyGIEV")) { ctrl->resetToDefault(true); }
        });
    }
    // [EnvIntensity] The classic/legacy note (updateGammaLabel) depends on
    // RenderSkyAutoAdjustLegacy, a viewer setting the environment change path
    // never signals, so listen to it directly. scoped_connection member:
    // disconnected automatically when the floater is destroyed. The note text
    // does not depend on the EV values themselves, so no listener on those.
    if (auto auto_adjust_ctrl = gSavedSettings.getControl("RenderSkyAutoAdjustLegacy"))
    {
        mEnvIntensityAutoAdjustConn = auto_adjust_ctrl->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateGammaLabel(); });
    }
    // [TonemapLegacySky] Refresh the note live; retain the listener in the
    // existing scoped-connection collection so floater destruction disconnects it.
    if (auto tonemap_legacy_ctrl = gSavedSettings.getControl("AlchemyTonemapLegacyGammaSkies"))
    {
        mEnvIntensityPresetConns.emplace_back(tonemap_legacy_ctrl->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateGammaLabel(); }));
    }

    // [EnvIntensity v2] Advanced exposure panel. Every slider / swatch / check
    // box binds through control_name; code wires only the reset buttons, the
    // Advanced show/hide toggle and the Sun/Moon link label. Nothing here
    // touches mLiveSky.
    for (const EnvIntensityResetBinding& rb : ENV_INTENSITY_RESETS)
    {
        if (LLUICtrl* btn = findChild<LLUICtrl>(rb.mButton))
        {
            const std::string setting(rb.mSetting);
            const std::string setting2(rb.mSetting2 ? rb.mSetting2 : "");
            btn->setCommitCallback([setting, setting2](LLUICtrl*, const LLSD&)
            {
                env_intensity_reset_setting(setting);
                if (!setting2.empty())
                {
                    env_intensity_reset_setting(setting2);
                }
            });
        }
    }
    if (LLUICtrl* reset_all = findChild<LLUICtrl>("env_adv_reset_all"))
    {
        reset_all->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            for (const char* key : ENV_INTENSITY_ADV_KEYS)
            {
                env_intensity_reset_setting(key);
            }
            // explicit: the moon follows the Sun slider again
            gSavedSettings.setBOOL("AlchemyEnvMoonLinked", true);
        });
    }
    if (LLUICtrl* toggle = findChild<LLUICtrl>("env_adv_toggle"))
    {
        toggle->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            gSavedSettings.setBOOL("AlchemyEnvIntensityAdvanced",
                                   !gSavedSettings.getBOOL("AlchemyEnvIntensityAdvanced"));
        });
    }
    // Panel state and the moon link are settings, so Debug Settings edits (and
    // the toggle / check box above) all flow through one listener each.
    if (auto adv_ctrl = gSavedSettings.getControl("AlchemyEnvIntensityAdvanced"))
    {
        mEnvIntensityAdvancedConn = adv_ctrl->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { applyEnvIntensityAdvanced(); });
    }
    if (auto moon_linked_ctrl = gSavedSettings.getControl("AlchemyEnvMoonLinked"))
    {
        mEnvIntensityMoonLinkedConn = moon_linked_ctrl->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateEnvIntensityMoonLink(); });
    }
    applyEnvIntensityAdvanced();
    updateEnvIntensityMoonLink();

    // [EnvIntensity presets] Quick Presets combo: apply-on-select, plus a
    // listener per watched key so a manual slider / swatch / Debug Settings
    // edit drops the combo to "Custom". refreshEnvIntensityPresetCombo() at
    // the end picks the entry (Stock, on a fresh viewer) that matches the
    // settings as postBuild found them.
    if (LLComboBox* preset_combo = findChild<LLComboBox>("env_intensity_preset"))
    {
        preset_combo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEnvIntensityPresetSelected(); });

        for (const char* name : ENV_INTENSITY_PRESET_WATCHED_KEYS)
        {
            if (auto ctrl = gSavedSettings.getControl(name))
            {
                mEnvIntensityPresetConns.emplace_back(ctrl->getSignal()->connect(
                    [this](LLControlVariable*, const LLSD&, const LLSD&) { refreshEnvIntensityPresetCombo(); }));
            }
        }

        // [EnvIntensity userpresets] user presets: "-- My presets --" section
        // after the static items, rebuilt on save / delete; an apply() from the
        // Presets floater (or from this combo) triggers one refresh when the 16
        // writes are done. Scoped connections: disconnected with the floater.
        mEnvIntensityPresetStaticCount = preset_combo->getItemCount();
        mEnvIntensityUserListConn = ALEnvIntensityPresets::connectListChanged(
            [this]()
            {
                rebuildEnvIntensityUserPresets();
                refreshEnvIntensityPresetCombo();
            });
        mEnvIntensityUserAppliedConn = ALEnvIntensityPresets::connectApplied(
            [this]() { refreshEnvIntensityPresetCombo(); });
        rebuildEnvIntensityUserPresets();
        refreshEnvIntensityPresetCombo();
    }
    if (LLUICtrl* presets_btn = findChild<LLUICtrl>("env_intensity_presets_btn"))
    {
        presets_btn->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            LLFloaterReg::showInstance("env_intensity_presets");
        });
    }

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

    // [EnvIntensity] Two sky states make the Light Intensity sliders (partly)
    // inert, mirroring LLSettingsVOSky::applySpecial:
    //  - classic sky (pre-PBR shading: a legacy sky with RenderSkyAutoAdjustLegacy
    //    off): both EV factors are inactive (1.0);
    //  - legacy-gamma sky (probe ambiance == 0): the Sun / Moon boost is capped
    //    at 0 EV, even with the [TonemapLegacySky] opt-in enabled.
    // Surface that instead of leaving dead sliders. Runs from refresh() (open +
    // every environment update), from the probe-ambiance slider, and from the
    // RenderSkyAutoAdjustLegacy / AlchemyTonemapLegacyGammaSkies change signals.
    if (LLUICtrl* note = findChild<LLUICtrl>("env_legacy_sun_note"))
    {
        const bool classic_sky  = mLiveSky->canAutoAdjust() && !should_auto_adjust();
        const bool legacy_gamma = (ambiance == 0.f);
        if (classic_sky)
        {
            note->setValue(getString("env_intensity_classic_note"));
            note->setToolTip(getString("env_intensity_classic_tooltip"));
        }
        else if (legacy_gamma)
        {
            // [TonemapLegacySky] The cap remains, but the opt-in enables tonemapping.
            static LLCachedControl<bool> tonemap_legacy_skies(gSavedSettings, "AlchemyTonemapLegacyGammaSkies", false);
            note->setValue(getString("env_intensity_legacy_note"));
            note->setToolTip(tonemap_legacy_skies()
                ? "Tonemapping is enabled for this Reflection Probe Ambiance 0 sky. Sun / Moon boosts remain capped at 0 EV; negative values still work. Set Reflection Probe Ambiance above 0 to enable boosts."
                : getString("env_intensity_legacy_tooltip"));
        }
        note->setVisible(classic_sky || legacy_gamma);
    }
}

// [EnvIntensity v2] Show / hide the Advanced panel per AlchemyEnvIntensityAdvanced
// and grow / shrink the floater by the panel's height plus the layout stack's
// panel spacing (mirrors the BDMerge strip pattern in postBuild), so the stock
// env_controls panel keeps its size. Idempotent: called from postBuild and from
// the setting's change signal.
void LLFloaterEnvironmentAdjust::applyEnvIntensityAdvanced()
{
    LLPanel* adv = findChild<LLPanel>(PANEL_ENV_INTENSITY_ADV);
    if (!adv)
    {
        return;
    }
    const bool show = gSavedSettings.getBOOL("AlchemyEnvIntensityAdvanced");

    if (LLButton* toggle = findChild<LLButton>("env_adv_toggle"))
    {
        toggle->setLabel(getString(show ? "env_intensity_adv_hide" : "env_intensity_adv_show"));
    }

    if (adv->getVisible() == show)
    {
        return;
    }
    const S32 delta = (ENV_INTENSITY_ADV_PANEL_HEIGHT + ENV_INTENSITY_PANEL_SPACING) * (show ? 1 : -1);

    if (isMinimized())
    {
        // Minimised: the floater is a title bar and reshape() would fight the
        // minimise rect. Adjust the remembered expanded size instead (kept
        // top-anchored) so the restore lands at the right height.
        LLRect expanded = getExpandedRect();
        const S32 new_height = llmax(expanded.getHeight() + delta, getMinHeight());
        expanded.mBottom = expanded.mTop - new_height;
        setExpandedRect(expanded);
        adv->setVisible(show);
        return;
    }

    adv->setVisible(show);
    // Never below min_height (the layout stack then keeps the stock panels intact).
    const S32 new_height = llmax(getRect().getHeight() + delta, getMinHeight());
    if (new_height != getRect().getHeight())
    {
        reshape(getRect().getWidth(), new_height);
    }
}

// [EnvIntensity v2] Linked (default): the Sun slider drives both bodies and its
// label reads "Sun / Moon (EV)"; the Moon slider is disabled. Unlinked: "Sun
// (EV)" and the Moon slider + its reset are live.
void LLFloaterEnvironmentAdjust::updateEnvIntensityMoonLink()
{
    const bool linked = gSavedSettings.getBOOL("AlchemyEnvMoonLinked");
    if (LLUICtrl* label = findChild<LLUICtrl>("env_sun_ev_label"))
    {
        label->setValue(getString(linked ? "env_intensity_sun_moon_label" : "env_intensity_sun_label"));
    }
    static const char* const moon_widgets[] = { "env_moon_ev", "env_moon_ev_unit", "env_moon_ev_reset" };
    for (const char* name : moon_widgets)
    {
        if (LLView* view = findChild<LLView>(name))
        {
            view->setEnabled(!linked);
        }
    }
}

// [EnvIntensity presets] env_intensity_preset commit: "Custom" (no matching
// preset, the combo's own placeholder) is not a preset to apply, so selecting
// it is a no-op; any real key looks up and applies ENV_INTENSITY_PRESETS.
void LLFloaterEnvironmentAdjust::onEnvIntensityPresetSelected()
{
    LLComboBox* combo = findChild<LLComboBox>("env_intensity_preset");
    if (!combo)
    {
        return;
    }
    const std::string key = combo->getValue().asString();
    if (key == "custom" || key.empty() || key == ENV_INTENSITY_USER_HEADER_VALUE)
    {
        // Picking "Custom" changes nothing; re-derive the label so an exact
        // preset match is not left showing "Custom". (An empty value / the
        // "-- My presets --" header are disabled rows; treated the same way in
        // case keyboard navigation lands on one.)
        refreshEnvIntensityPresetCombo();
        return;
    }
    // [EnvIntensity userpresets] "user:<name>" -> saved user preset
    if (key.compare(0, ENV_INTENSITY_USER_PREFIX.size(), ENV_INTENSITY_USER_PREFIX) == 0)
    {
        if (!ALEnvIntensityPresets::applyNamed(key.substr(ENV_INTENSITY_USER_PREFIX.size())))
        {
            // deleted behind the combo's back: resync the list and selection
            rebuildEnvIntensityUserPresets();
            refreshEnvIntensityPresetCombo();
        }
        return;
    }
    applyEnvIntensityPreset(key);
}

// [EnvIntensity presets] Writes the full deterministic set of AlchemyEnv*
// keys for `key` (see ENV_INTENSITY_PRESETS). The write itself is the shared
// ALEnvIntensityPresets::apply(): while it runs isApplying() suppresses
// refreshEnvIntensityPresetCombo() so the 16 individual setting-change signals
// do not flash "Custom" mid-write; when it finishes the "applied" signal calls
// refreshEnvIntensityPresetCombo() once (connected in postBuild), which
// re-derives the combo from the values just written (always an exact match).
void LLFloaterEnvironmentAdjust::applyEnvIntensityPreset(const std::string& key)
{
    const EnvIntensityPreset* preset = nullptr;
    for (const EnvIntensityPreset& p : ENV_INTENSITY_PRESETS)
    {
        if (key == p.mKey)
        {
            preset = &p;
            break;
        }
    }
    if (!preset)
    {
        return;
    }

    ALEnvIntensityPresets::apply(env_intensity_builtin_values(*preset));
}

// [EnvIntensity userpresets] Rebuilds the dynamic tail of the Quick Presets
// combo: [static XUI items][separator][-- My presets --][user presets, sorted].
// Nothing is added while there are no user presets. The selection is NOT
// restored here (add() may move it); callers follow with
// refreshEnvIntensityPresetCombo().
void LLFloaterEnvironmentAdjust::rebuildEnvIntensityUserPresets()
{
    LLComboBox* combo = findChild<LLComboBox>("env_intensity_preset");
    if (!combo || mEnvIntensityPresetStaticCount <= 0)
    {
        return;
    }

    while (combo->getItemCount() > mEnvIntensityPresetStaticCount)
    {
        if (!combo->remove(combo->getItemCount() - 1))
        {
            break;
        }
    }

    const std::vector<std::string> names = ALEnvIntensityPresets::listNames();
    if (names.empty())
    {
        return;
    }
    combo->addSeparator();
    // (translated skins that predate the string fall back to the English header)
    const std::string header = hasString("env_intensity_my_presets")
        ? getString("env_intensity_my_presets") : std::string("-- My presets --");
    combo->add(header, LLSD(ENV_INTENSITY_USER_HEADER_VALUE), ADD_BOTTOM, false);
    for (const std::string& name : names)
    {
        combo->add(name, LLSD(ENV_INTENSITY_USER_PREFIX + name));
    }
}

// [EnvIntensity presets] Re-derives the combo's selection from the live
// AlchemyEnv* settings: an exact match (within the shared 1e-4 epsilon, to
// absorb F32 round-trip through LLSD) selects that preset, otherwise the
// combo falls back to its "Custom" placeholder item. Built-in presets win
// ties over user presets (same values under two names). Called once from
// postBuild and from every watched setting's change signal; a no-op while
// ALEnvIntensityPresets::apply() is writing.
void LLFloaterEnvironmentAdjust::refreshEnvIntensityPresetCombo()
{
    if (ALEnvIntensityPresets::isApplying())
    {
        return;
    }
    LLComboBox* combo = findChild<LLComboBox>("env_intensity_preset");
    if (!combo)
    {
        return;
    }

    const ALEnvIntensityPresets::Values live = ALEnvIntensityPresets::currentValues();

    std::string matched_key("custom");
    bool found = false;
    for (const EnvIntensityPreset& p : ENV_INTENSITY_PRESETS)
    {
        if (ALEnvIntensityPresets::nearlyEqual(live, env_intensity_builtin_values(p)))
        {
            matched_key = p.mKey;
            found = true;
            break;
        }
    }
    if (!found)
    {
        const std::string user_name = ALEnvIntensityPresets::findMatchingName(live);
        if (!user_name.empty())
        {
            matched_key = ENV_INTENSITY_USER_PREFIX + user_name;
        }
    }
    combo->setValue(LLSD(matched_key));
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
