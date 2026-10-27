// Offline signal-test boundary: real LLControlGroup/LLControlVariable and the
// complete production listener registration run without a viewer or GL context.
#include "linden_common.h"
#include "llcontrol.h"
#include <boost/bind/bind.hpp>
#include <cassert>
#include <iostream>
using namespace boost::placeholders;

LLControlGroup gSavedSettings("env-only-signal-test");
LLControlGroup gSavedPerAccountSettings("env-only-account-test");
struct BumpImages { void destroyGL() {} void restoreGL() {} } gBumpImageList;
struct LLPipeline
{
    bool isInit() { return false; }
    static void refreshCachedSettings() {}
} gPipeline;
// Counts even direct setShaders calls in a registration lambda. No GL work.
struct LLViewerShaderMgr
{
    inline static U64 calls = 0;
    static LLViewerShaderMgr* instance() { static LLViewerShaderMgr mgr; return &mgr; }
    void setShaders() { ++calls; }
};
bool alDiopterIsPresetMaterializing() { return false; }
namespace ALKitHistory { bool replaying() { return false; } }
namespace ALSensorSettings
{
    struct Preset { float baseISO = 100.f; };
    Preset preset() { return {}; }
}
namespace ALExposureSettingsHistory { void initialize() {} }
namespace ALAreaShadow
{
    struct Quality { int samples = 1, steps = 1; bool half = false; };
    Quality quality(int) { return {}; }
}
namespace ALPanelWeatherSettings { void initializeLibraryWeather() {} }
namespace LLTerrainMaterials { constexpr U32 ASSET_COUNT = 4; }
namespace RlvSettingNames { const char* Main = "RestrainedLove"; }
namespace RlvSettings { void onChangedSettingMain(const LLSD&) {} }
namespace LLHUDText { void onFadeSettingsChanged(const LLSD&) {} }
bool validateAnisotropicFiltering(const LLSD&) { return true; }
// All controls are declared before registration. A missing name is a hard
// failure rather than an invented control or a silently skipped listener.
LLControlVariable* setting_get_control(LLControlGroup& group, const std::string& name)
{
    LLControlVariable* control = group.getControl(name);
    assert(control);
    return control;
}
