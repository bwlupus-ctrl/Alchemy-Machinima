/**
 * @file alweatheruimodel_test.cpp
 * @brief Weather commands and status revision tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alweatheruimodel.h"
namespace tut
{
    struct weather_ui_data {};
    typedef test_group<weather_ui_data> weather_ui_group;
    typedef weather_ui_group::object weather_ui_object;
    weather_ui_group weather_ui_tests("ALWeatherUIModel");
    template<> template<> void weather_ui_object::test<1>()
    {
        ALWeatherBridge::Settings s;
        const F32 rates[] = {1.f, 5.f, 20.f, 80.f};
        for (U32 i = 0; i < 4; ++i)
        {
            ALWeatherUIModel::applyPreset(s, i);
            ensure("preset rate", s.rainRate == rates[i]);
            ensure("preset activates local library", s.mode == 1 && s.local);
            ensure("preset leaves master off", !s.enabled);
        }
        s.enabled = true; s.storm = 0.8f;
        ALWeatherUIModel::stop(s);
        ensure("stop keeps mode and master", s.enabled && s.mode == 1);
        ensure("stop clears rain and storm targets", s.rainRate == 0.f && s.storm == 0.f);
        ALWeatherUIModel ui;
        ensure("status initial revision", ui.refresh(s, {}, false));
        const U64 revision = ui.revision();
        ensure("status no redundant writes", !ui.refresh(s, {}, false) && ui.revision() == revision);
        s.mode = 0;
        ensure("mode changes explain the library gate", ui.refresh(s, {}, false) && ui.status() == "Library mode: Existing rain - apply a preset");
        U32 seed = 0;
        ensure("full U32 seed is exact", ALWeatherUIModel::parseSeed("4294967295", seed) && seed == 4294967295U);
        ensure("overflow seed rejected", !ALWeatherUIModel::parseSeed("4294967296", seed));
        ensure("negative seed rejected", !ALWeatherUIModel::parseSeed("-1", seed));
        ensure("partial seed rejected", !ALWeatherUIModel::parseSeed("77x", seed));
        s.mode = 1;
        s.rainRate = 17.f;
        ALWeatherUIModel::applyPreset(s, 4);
        ensure("Custom keeps the entered rate", s.rainRate == 17.f);
        SLWeather::State state;
        ui.refresh(s, state, false, 1.0);
        state.rainFall = 10.f;
        ensure("numeric readout capped at 10 Hz", !ui.refresh(s, state, false, 1.01));
        ensure("numeric readout refreshes on next interval", ui.refresh(s, state, false, 1.11));
    }
    template<> template<> void weather_ui_object::test<2>()
    {
        ALWeatherBridge::Settings s;
        const F32 rates[] = {0.3f, 1.5f, 5.f, 12.f};
        for (U32 i = 0; i < 4; ++i)
        {
            ALWeatherUIModel::applyPreset(s, i + 5);
            ensure("snow preset selects snowfall only", s.snowEnabled && s.precipType == 1 && s.snowRate == rates[i]);
            ensure("snow preset never enables secondary effects", !s.visibility && !s.persistentWet && !s.puddles);
            ensure("snow preset leaves master off", !s.enabled);
        }
        ensure("Blizzard declares weather wind at least 15 m/s", s.windSource == 1 && s.windSpeed >= 15.f);
        ALWeatherUIModel ui;
        s.enabled = true;ui.refresh(s, {}, false);
        ensure("snow status has units", ui.status().find("cm/h") != std::string::npos);
        ALWeatherUIModel::stop(s);
        ensure("stop clears both rates", s.rainRate == 0.f && s.snowRate == 0.f);
        ALWeatherUIModel::applyPreset(s, 1);
        ensure("rain preset selects rain layer", !s.snowEnabled && s.precipType == 0);
    }
    template<> template<> void weather_ui_object::test<3>()
    {
        ALWeatherBridge::Settings s;
        ensure("master blocker", std::string(ALWeatherUIModel::snowRequirement(s)).find("Weather controls off") == 0);
        s.enabled = true;
        ensure("mode blocker", std::string(ALWeatherUIModel::snowRequirement(s)).find("Library mode: Existing rain") == 0);
        s.mode = 1;
        ensure("source blocker", std::string(ALWeatherUIModel::snowRequirement(s)).find("Local weather off") == 0);
        s.local = true;
        ensure("snow enable blocker", std::string(ALWeatherUIModel::snowRequirement(s)).find("Falling snow off") == 0);
        s.snowEnabled = true;
        ensure("type blocker", std::string(ALWeatherUIModel::snowRequirement(s)).find("Precipitation type is Rain") == 0);
        s.precipType = 1; s.snowRate = 0.f;
        ensure("rate blocker", std::string(ALWeatherUIModel::snowRequirement(s)).find("Snow rate is zero") == 0);
        ALWeatherUIModel::applyPreset(s, 7);
        ensure("Heavy snow opens every snowfall setting gate", ALWeatherUIModel::snowRequirement(s) == nullptr);
    }
}
