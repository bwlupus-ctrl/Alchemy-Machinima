"""Compile the production idle weather block, panel preparation and settings reader.

Only viewer services are fixtures. Both binding and GL checks use this translation
unit, so disconnecting the llappviewer.cpp call removes their simulation input.
"""
from pathlib import Path


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
    initialization = ''
    initialize_call = ''
    if 'void ALPanelWeatherSettings::initializeLibraryWeather()' in panel:
        initialization = (function(panel, 'const std::vector<std::string>& ALPanelWeatherSettings::trackedSettings()') +
                          function(panel, 'void refreshInactiveLibraryWeather()') +
                          function(panel, 'void ALPanelWeatherSettings::initializeLibraryWeather()'))
        initialize_call = '\n'.join(line for line in function(
            tree.read('indra/newview/llviewercontrol.cpp'), 'void settings_setup_listeners()').splitlines()
            if 'ALPanelWeatherSettings::initializeLibraryWeather();' in line)
    prefix = r'''
#include "alweatheruimodel.h"
#include <algorithm>
#include <map>
#include <string>
#include <cmath>
#include <functional>
#include <vector>
namespace WeatherPreparationFixture {
U64 reads=0, terrain=0, refreshes=0, notifications=0;
struct LLSD {};
struct LLControlVariable {
 std::vector<std::function<void()>>* listeners;
 struct Signal {LLControlVariable* control;template<class F>void connect(F f){auto* c=control;c->listeners->push_back([c,f](){f(c,{},{});});}} signal{this};
 Signal* getSignal(){return &signal;}
};
struct Settings {
 std::map<std::string,double> values;
 std::map<std::string,std::vector<std::function<void()>>> listeners;
 std::map<std::string,LLControlVariable> controls;
 LLControlVariable* getControl(const std::string& n){auto& c=controls[n];c.listeners=&listeners[n];return &c;}
 U32 getU32(const std::string& n){return static_cast<U32>(values[n]);}
 bool getBOOL(const std::string& n){return values[n]!=0;}
 void setBOOL(const std::string& n,bool v){values[n]=v;}
} gSavedSettings;
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
struct Region {U64 getHandle()const{return 1;}};
struct Agent {LLVector3 getPositionAgent()const{return {};}
 Region* getRegion(){static Region r;return &r;}} gAgent;
struct LLWorld {static LLWorld* getInstance(){static LLWorld w;return &w;}
 float resolveLandHeightAgent(const LLVector3&){++terrain;return 0;}};
struct Sky {float getCloudShadow()const{return 1.f;}};
struct LLEnvironment {static LLEnvironment& instance(){static LLEnvironment e;return e;}
 Sky* getCurrentSky(){static Sky s;return &s;}};
struct LLViewerShaderMgr {static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}
 bool weatherMaterialsSupported()const{return true;}};
struct LLPipeline {static constexpr bool sRenderDeferred=true;
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
 static Signal& libraryChanged(){static Signal s;return s;}};
'''
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
    suffix = r'''
void idle(){
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
'''.replace('IDLE_BLOCK', idle).replace('INITIALIZE_CALL', initialize_call)
    path = Path(out) / 'preparation.cpp'
    path.write_text(prefix + reader + initialization + update + suffix, encoding='utf-8')
    return path
