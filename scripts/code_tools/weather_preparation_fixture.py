"""Compile the production idle weather block, panel preparation and settings reader.

Only viewer services are fixtures. Both binding and GL checks use this translation
unit, so disconnecting the llappviewer.cpp call removes their simulation input.
"""
from pathlib import Path
import re


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def source(tree, out, mutation=None, particles=False):
    panel = tree.read('indra/newview/alpanelweathersettings.cpp')
    idle = function(tree.read('indra/newview/llappviewer.cpp'), 'void LLAppViewer::idle()')
    # Preserve the entire production weather block, including an absent/commented
    # call. Never manufacture a call when production has disconnected it.
    start = idle.index('    // Weather library freezes once here,')
    end = idle.index('    // [Director Switcher]', start)
    idle = idle[start:end]
    if mutation == 'idle-disconnect':
        assert 'ALPanelWeatherSettings::updateLibraryWeather();' in idle
        idle = idle.replace('ALPanelWeatherSettings::updateLibraryWeather();', '')
    reader = function(panel, 'ALWeatherBridge::Settings readLibrarySettings()')
    if mutation == 'setting':
        reader = reader.replace('s.rainRate = c_rainRate();', 's.rainRate = 5.f;')
    if mutation == 'surface-setting':
        reader = reader.replace('s.wetAmount = c_wetAmount();', 's.wetAmount = 0.5f;')
    reader = reader.replace('{', '{\n        ++reads;', 1)
    update = function(panel, 'void ALPanelWeatherSettings::updateLibraryWeather()')
    preset = function(panel, 'void ALPanelWeatherSettings::applyLibraryPreset(')
    initialization = function(panel, 'const std::vector<std::string>& ALPanelWeatherSettings::trackedSettings()')
    initialize_call = ''
    if 'void ALPanelWeatherSettings::initializeLibraryWeather()' in panel:
        initialization += ((function(panel, 'void ALPanelWeatherSettings::sanitizeLightningDistances()') if 'void ALPanelWeatherSettings::sanitizeLightningDistances()' in panel else '') +
                          function(panel, 'void refreshInactiveLibraryWeather()') +
                          function(panel, 'void ALPanelWeatherSettings::initializeLibraryWeather()'))
        initialize_call = '\n'.join(line for line in function(
            tree.read('indra/newview/llviewercontrol.cpp'), 'void settings_setup_listeners()').splitlines()
            if 'ALPanelWeatherSettings::initializeLibraryWeather();' in line)
    batch_class = function(tree.read('indra/newview/alpanelweathersettings.h'), 'class SettingsBatch') + ';' if 'class SettingsBatch' in tree.read('indra/newview/alpanelweathersettings.h') else ''
    batch_methods = ''.join(function(panel, 'void ALPanelWeatherSettings::' + name + '(') for name in ('beginSettingsBatch', 'endSettingsBatch', 'suspendLibraryWeather') if 'void ALPanelWeatherSettings::' + name + '(' in panel)
    if batch_class:
        initialization = batch_methods + initialization
    else:
        initialization = "void ALPanelWeatherSettings::beginSettingsBatch(){}void ALPanelWeatherSettings::endSettingsBatch(){}" + initialization
    prefix = r'''
#include "alweatheruimodel.h"
#include <algorithm>
#include <map>
#include <string>
#include <cmath>
#include <functional>
#include <vector>
#include <stdexcept>
namespace WeatherPreparationFixture {
U64 reads=0, terrain=0, refreshes=0, notifications=0;
struct LLSD {};
#define llassert(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
U32 sSettingsBatchDepth=0;bool sLightningDistancesDirty=false;
struct LLControlVariable {
 std::string name; std::vector<std::function<void()>>* listeners;
 void resetToDefault(bool);
 struct Signal {LLControlVariable* control;template<class F>void connect(F f){auto* c=control;c->listeners->push_back([c,f](){f(c,{},{});});}} signal{this};
 Signal* getSignal(){return &signal;}
};
struct Settings {
 std::map<std::string,double> values, defaults;
 std::map<std::string,std::vector<std::function<void()>>> listeners;
 std::map<std::string,LLControlVariable> controls;
 LLControlVariable* getControl(const std::string& n){auto& c=controls[n];c.name=n;c.listeners=&listeners[n];return &c;}
 U32 getU32(const std::string& n){return static_cast<U32>(values[n]);}
 void setU32(const std::string& n,U32 v){write(n,v);}
 bool getBOOL(const std::string& n){return values[n]!=0;}
 float getF32(const std::string& n){return float(values[n]);}
 void write(const std::string& n,double v){if(values[n]==v)return;values[n]=v;for(auto& f:listeners[n])f();}
 void setF32(const std::string& n,float v){write(n,v);}
 void setBOOL(const std::string& n,bool v){write(n,v);}
} gSavedSettings;
void LLControlVariable::resetToDefault(bool){gSavedSettings.setF32(name,float(gSavedSettings.defaults[name]));}
void setting_setup_signal_listener(Settings& settings,const std::string& name,std::function<void()> callback){
 settings.listeners[name].push_back(callback);
}
template<class T> struct LLCachedControl {
 std::string name;T fallback;LLCachedControl(Settings&,const char* n,T f):name(n),fallback(f){}
 T operator()()const{auto it=gSavedSettings.values.find(name);return it==gSavedSettings.values.end()?fallback:static_cast<T>(it->second);}
};
template<class T>T llclamp(T v,T lo,T hi){return std::clamp(v,lo,hi);}
float lerp(float a,float b,float t){return a+(b-a)*t;}
constexpr int VX=0,VY=1,VZ=2;
struct LLVector3 {float mV[3]{};} gWindVec{{1,2,0}};
bool gDisconnected=false,worldExists=true,regionExists=true;int teleportState=0;
struct Region {U64 getHandle()const{return 1;}};
struct LLAgent {enum {TELEPORT_NONE=0};};
struct Agent {int getTeleportState()const{return teleportState;}LLVector3 getPositionAgent()const{return {};}
 Region* getRegion(){static Region r;return regionExists?&r:nullptr;}} gAgent;
struct LLWorld {static bool instanceExists(){return worldExists;}static LLWorld* getInstance(){static LLWorld w;return &w;}
 float resolveLandHeightAgent(const LLVector3&){++terrain;return 0;}};
struct Sky {float getCloudShadow()const{return 1.f;}};
struct LLEnvironment {static LLEnvironment& instance(){static LLEnvironment e;return e;}
 Sky* getCurrentSky(){static Sky s;return &s;}};
struct LLViewerShaderMgr {static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}
 bool weatherMaterialsSupported()const{return true;}void setShaders(){++reloads;}inline static U64 reloads=0;};
struct LLPipeline {void suspendWeather(){++suspensions;}U64 suspensions=0;static constexpr bool sRenderDeferred=true;
 struct Surfaces {void* advanced=nullptr;void prepare(U32 n){PREPARE_BASIC}const std::string& status()const{static std::string s="Surface cover ready";return s;}} mWeatherSurfaces;
 struct Shelter {void prepare(){PREPARE_SHELTER}void suspend(){SUSPEND_SHELTER} const std::string& status()const{static std::string s="Advanced shelter ready";return s;}} mWeatherShelter;
 struct Particles {const char* status()const{return "Falling snow off";} void prepare(){PREPARE_PARTICLES}} mWeatherParticles;} gPipeline;
U64 gFrameCount=0;
struct LLPresentationTime {struct Frame {double presentation_time=0;};
 inline static double seconds=0;static Frame currentFrame(){return {seconds};}};
struct CountedUI:ALWeatherUIModel {
 bool refresh(const ALWeatherBridge::Settings& s,const SLWeather::State& state,bool catchup,double time){
  ++refreshes;return ALWeatherUIModel::refresh(s,state,catchup,time);}
};
PRODUCTION_GLOBALS
struct ALWeatherParticles {static bool supported(){PARTICLE_SUPPORTED}};
struct Signal {void operator()(){++notifications;}};
struct ALPanelWeatherSettings {static void updateLibraryWeather();static void initializeLibraryWeather();
 static const std::vector<std::string>& trackedSettings();
 static void sanitizeLightningDistances();
 static void beginSettingsBatch();static void endSettingsBatch();static void suspendLibraryWeather();
 BATCH_CLASS
 static void resetAllSettingsToDefaults();
 static void applyLibraryPreset(U32);
 static Signal& libraryChanged(){static Signal s;return s;}};
'''
    prefix = prefix.replace('BATCH_CLASS', batch_class)
    from weather_compute_fixture import declaration
    consumers = reader + initialization + update
    prefix = prefix.replace('PRODUCTION_GLOBALS', '\n'.join(declaration(panel, n) for n in ('sLibraryUI','sSurfaceStatus','sParticleStatus') if n in consumers))
    prefix = prefix.replace('ALWeatherUIModel sLibraryUI;', 'CountedUI sLibraryUI;')
    prefix = prefix.replace('PREPARE_BASIC','fixture_prepare_basic(n);' if particles else '')
    prefix = prefix.replace('PREPARE_SHELTER','fixture_prepare_shelter();' if particles else '').replace('SUSPEND_SHELTER','fixture_suspend_shelter();' if particles else '')
    prefix = prefix.replace('PREPARE_PARTICLES', 'fixture_prepare_particles();' if particles else '')
    prefix = prefix.replace('PARTICLE_SUPPORTED', 'return fixture_particle_supported()!=0;' if particles else 'return true;')
    if particles:
        prefix = 'extern "C" void fixture_prepare_basic(unsigned);\nextern "C" void fixture_prepare_shelter();\nextern "C" void fixture_suspend_shelter();\nextern "C" void fixture_prepare_particles();\nextern "C" int fixture_particle_supported();\n' + prefix
    control = tree.read('indra/newview/llviewercontrol.cpp')
    registration = function(control, 'void settings_setup_listeners()')
    # Keep whole weather registration calls, including multiline lambdas.
    calls = []
    for match in re.finditer(r'\bsetting_setup_signal_listener\s*\(', registration):
        start = match.start(); end = match.end(); depth = 1
        while depth:
            depth += (registration[end] == '(') - (registration[end] == ')'); end += 1
        call = registration[start:end]
        if 'AlchemyWeather' in call: calls.append(call + ';')
    shader_listeners = function(control, 'static bool handleSetShaderChanged(').replace('{', '{\n++handleChanges;', 1)
    if 'static void setupWeatherMaterialListeners()' in control:
        shader_listeners += function(control, 'static void setupWeatherMaterialListeners()')
    shader_listeners += '\nvoid registerShaderListeners(){\n' + '\n'.join(calls) + '\n' + '\n'.join(line for line in registration.splitlines() if 'setupWeatherMaterialListeners();' in line) + '\n}\n'
    initialize_call = 'registerShaderListeners();\n' + initialize_call
    prefix = prefix.replace('struct LLSD {};', 'struct LLSD {};U64 handleChanges=0;std::vector<std::function<void()>> pending;void doOnIdleOneTime(std::function<void()> f){pending.push_back(f);}')
    prefix = prefix.replace('void setting_setup_signal_listener(Settings& settings,const std::string& name,std::function<void()> callback){\n settings.listeners[name].push_back(callback);\n}', r'''
template<class F>void setting_setup_signal_listener(Settings& settings,const std::string& name,F callback){
 settings.listeners[name].push_back([callback](){if constexpr(std::is_invocable_v<F>)callback();else callback(LLSD{});});
}
struct Bumps {void destroyGL(){}void restoreGL(){}}gBumpImageList;
''')
    prefix = prefix.replace('#include <stdexcept>', '#include <stdexcept>\n#include <type_traits>')
    prefix = prefix.replace('void suspendWeather(){', 'bool isInit(){return true;}static void refreshCachedSettings(){}void suspendWeather(){')
    reset = function(panel, 'void ALPanelWeatherSettings::resetAllSettingsToDefaults()')
    suffix = r'''
void idle(){
 auto work=std::move(pending);pending.clear();for(auto& f:work)f();
IDLE_BLOCK
}
void initialize(){static bool initialized=false;if(!initialized){initialized=true;INITIALIZE_CALL}}
}
extern "C" __declspec(dllexport) void fixture_weather_set(const char* key,double v){
 using namespace WeatherPreparationFixture;
 initialize();
 if(gSavedSettings.values.count(key) && gSavedSettings.values[key]==v)return;
 gSavedSettings.values[key]=v;
 for(auto& listener:gSavedSettings.listeners[key])listener();
}
extern "C" __declspec(dllexport) void fixture_weather_step(double time){
 using namespace WeatherPreparationFixture;
 initialize();
 ++gFrameCount;LLPresentationTime::seconds=time;idle();
}
extern "C" __declspec(dllexport) void fixture_weather_restart(){
 using namespace WeatherPreparationFixture;
 const auto saved=gSavedSettings.values;
 gSavedSettings.values["AlchemyWeatherEnabled"]=0;
 fixture_weather_step(0.);
 gSavedSettings.values=saved;
}
extern "C" __declspec(dllexport) void fixture_weather_counts(double* out){
 using namespace WeatherPreparationFixture;
 out[0]=double(reads);out[1]=double(terrain);out[2]=double(refreshes);out[3]=double(notifications);
}
extern "C" __declspec(dllexport) const char* fixture_weather_status(){
 using namespace WeatherPreparationFixture;initialize();return sLibraryUI.status().c_str();
}
extern "C" __declspec(dllexport) void fixture_weather_load(const char* key,double value){
 WeatherPreparationFixture::gSavedSettings.values[key]=value;
}
extern "C" __declspec(dllexport) void fixture_weather_preset(int preset){
 using namespace WeatherPreparationFixture;initialize();ALPanelWeatherSettings::applyLibraryPreset(U32(preset));
}
'''.replace('IDLE_BLOCK', idle).replace('INITIALIZE_CALL', initialize_call)
    suffix += r'''
extern "C" __declspec(dllexport) void fixture_weather_default(const char* key,double value){WeatherPreparationFixture::gSavedSettings.defaults[key]=value;}
extern "C" __declspec(dllexport) double fixture_weather_get(const char* key){return WeatherPreparationFixture::gSavedSettings.values[key];}
extern "C" __declspec(dllexport) void fixture_weather_batch(int begin){using namespace WeatherPreparationFixture;if(begin)ALPanelWeatherSettings::beginSettingsBatch();else ALPanelWeatherSettings::endSettingsBatch();}
extern "C" __declspec(dllexport) void fixture_weather_reset(){using namespace WeatherPreparationFixture;initialize();ALPanelWeatherSettings::resetAllSettingsToDefaults();}
extern "C" __declspec(dllexport) void fixture_weather_lifecycle(int disconnected,int world,int region,int teleport){using namespace WeatherPreparationFixture;gDisconnected=disconnected!=0;worldExists=world!=0;regionExists=region!=0;teleportState=teleport;}
'''
    suffix += r'''
extern "C" __declspec(dllexport) void fixture_weather_reload_counts(double* out){using namespace WeatherPreparationFixture;out[0]=double(LLViewerShaderMgr::reloads);out[1]=double(handleChanges);}
'''
    path = Path(out) / 'preparation.cpp'
    path.write_text(prefix + shader_listeners + reader + initialization + reset + preset + update + suffix, encoding='utf-8')
    return path
