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


def source(tree, out, mutation=None):
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
    prefix = r'''
#include "alweatheruimodel.h"
#include <algorithm>
#include <map>
#include <string>
#include <cmath>
namespace WeatherPreparationFixture {
U64 reads=0, terrain=0, refreshes=0, notifications=0;
struct Settings {
 std::map<std::string,double> values;
 U32 getU32(const std::string& n){return static_cast<U32>(values[n]);}
 bool getBOOL(const std::string& n){return values[n]!=0;}
 void setBOOL(const std::string& n,bool v){values[n]=v;}
} gSavedSettings;
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
 struct Surfaces {std::string status()const{return "Surface cover ready";}} mWeatherSurfaces;} gPipeline;
U64 gFrameCount=0;
struct LLPresentationTime {struct Frame {double presentation_time=0;};
 inline static double seconds=0;static Frame currentFrame(){return {seconds};}};
struct CountedUI:ALWeatherUIModel {
 bool refresh(const ALWeatherBridge::Settings& s,const SLWeather::State& state,bool catchup,double time){
  ++refreshes;return ALWeatherUIModel::refresh(s,state,catchup,time);}
} sLibraryUI;
std::string sSurfaceStatus;
struct Signal {void operator()(){++notifications;}};
struct ALPanelWeatherSettings {static void updateLibraryWeather();
 static Signal& libraryChanged(){static Signal s;return s;}};
'''
    suffix = r'''
void idle(){
IDLE_BLOCK
}
}
extern "C" __declspec(dllexport) void fixture_weather_set(const char* key,double v){
 WeatherPreparationFixture::gSavedSettings.values[key]=v;
}
extern "C" __declspec(dllexport) void fixture_weather_step(double time){
 using namespace WeatherPreparationFixture;
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
'''.replace('IDLE_BLOCK', idle)
    path = Path(out) / 'preparation.cpp'
    path.write_text(prefix + reader + update + suffix, encoding='utf-8')
    return path
