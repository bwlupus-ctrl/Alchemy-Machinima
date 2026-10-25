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
        ensure("mode changes observable status", ui.refresh(s, {}, false) && ui.status() == "Existing rain");
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
}
