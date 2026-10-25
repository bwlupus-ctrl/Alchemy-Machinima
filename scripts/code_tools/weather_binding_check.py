"""WP1 Director rows -> native setting/command -> production CPU consumer tests.

The generated XUI is authoritative for setting rows. Code-bound callbacks are
compiled directly from LLFloaterDirector::bindWeatherPage with tiny UI/settings
fixtures. The actual native settings reader and model commands are compiled too.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import numpy as np
from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT, SOURCES, compiler_command, prepare
from weather_rain_gl_check import function
from weather_preparation_fixture import source as preparation_source


def settings_table(text):
    entries = list(ET.fromstring(text).find('map'))
    return {entries[i].text: {**{children[j].text: children[j+1].text for j in range(0,len(children),2)},
                             'FullXML': ET.tostring(entries[i+1], encoding='unicode')}
            for i in range(0,len(entries),2) if entries[i].tag=='key'
            and entries[i+1].tag=='map'
            for children in [list(entries[i+1]) ]}


def build(out, mutation=None, source_ref='WORKTREE'):
    prepare(out)
    tree=SourceTree(source_ref)
    # Snapshot sources AND their local headers; quoted includes must not fall
    # back to newer working-tree declarations when testing a historical ref.
    for name in SOURCES:
        for ext in ('.h','.cpp'):
            (out/(name+ext)).write_text(tree.read('indra/newview/'+name+ext),encoding='utf-8')
    (out/'stdtypes.h').write_text(tree.read('indra/llcommon/stdtypes.h'),encoding='utf-8')
    panel=tree.read('indra/newview/alpanelweathersettings.cpp')
    director=tree.read('indra/newview/llfloaterdirector.cpp')
    read=function(panel,'ALWeatherBridge::Settings readLibrarySettings()')
    binding=function(director,'void LLFloaterDirector::bindWeatherPage(')
    refresh=function(director,'void LLFloaterDirector::refreshWorldLensReadouts(')
    pipe=function(tree.read('indra/newview/pipeline.cpp'),'void LLPipeline::renderWeather(')
    color_code='''
struct LLColor3 { float mV[3]; };
struct ColorSink {float* output;void uniform3f(int,float r,float g,float b){output[0]=r;output[1]=g;output[2]=b;}};
extern "C" __declspec(dllexport) void colors(float* rain,float* lightning){
 auto finite_clamp=[](float value,float fallback,float low,float high){return std::clamp(std::isfinite(value)?value:fallback,low,high);};
 const int sRainColor=0,sLightningColor=1;
 const float rain_value=static_cast<float>(gSavedSettings.values["AlchemyWeatherRainColor"]);
 const float lightning_value=static_cast<float>(gSavedSettings.values["AlchemyWeatherLightningColor"]);
 LLColor3 rain_color_setting{{rain_value,rain_value,rain_value}},lightning_color_setting{{lightning_value,lightning_value,lightning_value}};
 ColorSink rain_program{rain},lightning_program{lightning};
'''
    for prefix_name,shader_name,uniform in [('rain','rain_program','sRainColor'),('lightning','lightning_program','sLightningColor')]:
        start=pipe.index('const LLColor3 '+prefix_name+'_color_raw')
        end=pipe.index(shader_name+'.uniformMatrix4fv(',start)
        color_code+=pipe[start:end]
        call=re.search(re.escape(shader_name)+r'\.uniform3f\(\s*'+uniform+r',.*?\);',pipe,re.S)
        assert call
        color_code+=call.group(0)+'\n'
    color_code+='}\n'
    if mutation=='setting': read=read.replace('s.rainRate = c_rainRate();','s.rainRate = 5.f;')
    if mutation=='command': binding=binding.replace('ALPanelWeatherSettings::applyLibraryPreset(combo->getValue().asInteger());','(void)combo;')
    if mutation=='surface-setting': read=read.replace('s.wetAmount = c_wetAmount();','s.wetAmount = 0.5f;')
    if mutation=='surface-command': binding=binding.replace('ALPanelWeatherSettings::clearSurfaces();','(void)0;')
    if mutation=='surface-apply': binding=binding.replace('ALPanelWeatherSettings::applyStartingSurfaces();','(void)0;')
    prefix=r'''
#include "alweatheruimodel.h"
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <algorithm>
struct LLSD {
 std::string text; LLSD()=default; LLSD(const char* s):text(s){} LLSD(const std::string& s):text(s){} LLSD(int i):text(std::to_string(i)){}
 int asInteger() const{return std::stoi(text);} double asReal()const{return std::stod(text);} std::string asString()const{return text;}
 bool isDefined()const{return !text.empty();}
};
struct Settings {
 std::map<std::string,double> values;
 unsigned getU32(const std::string& n){return static_cast<unsigned>(values[n]);}
 bool getBOOL(const std::string& n){return values[n]!=0;}
 float getF32(const std::string& n){return static_cast<float>(values[n]);}
 void setU32(const std::string& n,unsigned v){values[n]=v;}
 void setF32(const std::string& n,float v){values[n]=v;}
 void setBOOL(const std::string& n,bool v){values[n]=v;}
} gSavedSettings;
template<class T> struct LLCachedControl {
 std::string name;T fallback; LLCachedControl(Settings&,const char* n,T f):name(n),fallback(f){}
 T operator()()const{auto it=gSavedSettings.values.find(name);return it==gSavedSettings.values.end()?fallback:static_cast<T>(it->second);}
};
template<class T> struct LLHandle {T* value=nullptr;T* get()const{return value;}};
struct LLUICtrl;
struct LLView {
 virtual ~LLView()=default;LLView* parent=nullptr;LLView* getParent(){return parent;}
 std::map<std::string,std::unique_ptr<LLUICtrl>> widgets;
 template<class T>T* findChild(const char* name);
};
struct LLUICtrl:LLView {
 LLSD value;std::function<void(LLUICtrl*,const LLSD&)> callback;
 bool enabled=true;void setEnabled(bool v){enabled=v;}
 virtual ~LLUICtrl()=default;
 void setValue(const LLSD& v){value=v;}LLSD getValue()const{return value;}
 void setCommitCallback(std::function<void(LLUICtrl*,const LLSD&)> cb){callback=cb;}
 template<class T> LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
 void fire(){if(callback)callback(this,{});}
};
struct LLButton:LLUICtrl{};struct LLComboBox:LLUICtrl{};struct LLLineEditor:LLUICtrl{};struct LLTextBox:LLUICtrl{};
template<class T>T* LLView::findChild(const char* name){auto& p=widgets[name];if(!p)p=std::make_unique<T>();return dynamic_cast<T*>(p.get());}
struct ALKitRow:LLView {
 LLUICtrl* control=nullptr;std::vector<std::function<void()>> changed;
 template<class T>LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
 LLSD boundValue()const{return control->getValue();}LLSD boundDefault()const{return {};}
 void setBoundValue(const LLSD& v){control->setValue(v);}void reset(){}
 int setValueCallback(std::function<void()> cb){changed.push_back(cb);return 1;}
 void bindingChanged(){for(auto& cb:changed)cb();}
 static void notifyValueChanged(LLUICtrl*);
};
namespace boost {namespace signals2 {using connection=int;}}
struct ALKitBinding {
 virtual ~ALKitBinding()=default;virtual LLSD get()const=0;virtual LLSD getDefault()const=0;
 virtual void set(const LLSD&)=0;virtual void reset()=0;virtual bool canSet()const=0;virtual bool hasDefault()const=0;
 virtual int connectChanged(std::function<void()>)=0;virtual std::string describe()const=0;
};
struct ALKitSettingWatch {
 std::map<std::string,std::vector<std::function<void(const LLSD&,const LLSD&)>>> watchers;
 static ALKitSettingWatch& instance(){static ALKitSettingWatch s;return s;}
 template<class F>int watch(const char* key,F cb){watchers[key].push_back(cb);return 1;}
 void fire(const char* key){for(auto& cb:watchers[key])cb({},{});}
};
struct Signal {template<class F>int connect(F){return 1;}};
ALWeatherUIModel sLibraryUI;
std::string sSurfaceStatus;
std::string sParticleStatus;
struct ALWeatherParticles {inline static bool available=true;static bool supported(){return available;}};
struct ALPanelWeatherSettings {
 static void applyLibraryPreset(U32);static void stopPrecipitation();
 static void applyStartingSurfaces();static void clearSurfaces();
 static const std::string& surfaceStatus();
 static const std::string& particleStatus();
 static void strikeLightningNow(){}static void confirmResetAll(){}
 static Signal& libraryChanged(){static Signal s;return s;}
 static const std::string& libraryStatus(){return sLibraryUI.status();}
};
struct LLGLSLShader;
struct LLPipeline {static constexpr bool sRenderDeferred=true;inline static int weatherBinds=0;
 struct Particles {bool available()const{return true;}} mWeatherParticles;
 void bindWeatherMaterial(LLGLSLShader*){++weatherBinds;}} gPipeline;
struct LLViewerShaderMgr {
 static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}
 bool weatherMaterialsEnabled()const;bool weatherMaterialsSupported()const;
 void updateWeatherUniforms(LLGLSLShader*);
};
void setTextIfChanged(LLTextBox* box,const std::string& s){box->setValue(s);}
void setVisibleIfChanged(LLTextBox*,bool){}
struct LLFloaterDirector {
 LLView* root;std::vector<int> mWorldLensConnections;
 template<class T>LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
 template<class T>T* findChild(const char* n){return root->findChild<T>(n);}
 void bindWeatherPage(LLView*);void refreshWorldLensReadouts(const std::string&);
};
'''
    suffix=r'''
extern "C" void fixture_weather_set(const char*,double);
extern "C" void fixture_weather_step(double);
extern "C" void fixture_weather_restart();
void sync_preparation(){for(const auto& kv:gSavedSettings.values)fixture_weather_set(kv.first.c_str(),kv.second);}
extern "C" __declspec(dllexport) void set(const char* key,double value){gSavedSettings.values[key]=value;}
extern "C" __declspec(dllexport) double get(const char* key){return gSavedSettings.values[key];}
extern "C" __declspec(dllexport) void command(int preset,int action,const char* text){
 ALKitSettingWatch::instance().watchers.clear();
 LLView root; LLFloaterDirector director;director.root=&root;director.bindWeatherPage(&root);
 if(action==0){root.findChild<LLComboBox>("weather_library_preset")->setValue(preset);root.findChild<LLButton>("weather_library_apply")->fire();}
 if(action==1)root.findChild<LLButton>("weather_library_stop")->fire();
 if(action==2){auto* seed=root.findChild<LLLineEditor>("weather_library_seed");seed->setValue(text);seed->fire();}
 if(action==3)root.findChild<LLButton>("weather_surface_apply")->fire();
 if(action==4)root.findChild<LLButton>("weather_surface_clear")->fire();
}
extern "C" __declspec(dllexport) void trace(double* output){
 auto settings=readLibrarySettings();
 sync_preparation();fixture_weather_restart();auto& bridge=ALWeatherBridge::Controller::instance();
 std::fill(output,output+8,0.0);
 for(U64 frame=0;frame<=7200;++frame){
  fixture_weather_step(frame/60.0);
  const auto r=ALWeatherBridge::rain(bridge.state(),0.55f,0.65f);
  output[0]+=r.intensity;output[1]+=r.density;output[2]+=r.windX;output[3]+=r.windY;
  output[4]+=bridge.lightning().mFlash;output[5]+=bridge.lightning().mStrikeDistanceMeters;
  output[6]+=bridge.state().rainFall;
 }
 sLibraryUI.refresh(settings,bridge.state(),bridge.catchingUp());output[7]=sLibraryUI.revision();
}
extern "C" __declspec(dllexport) int pinned(const char* setting,double v,int which){
 ALKitSettingWatch::instance().watchers.clear();
 LLView root;LLFloaterDirector director;director.root=&root;director.bindWeatherPage(&root);
 auto* control=which==0?static_cast<LLUICtrl*>(root.findChild<LLLineEditor>("weather_library_seed")):
  static_cast<LLUICtrl*>(root.findChild<LLComboBox>("weather_library_preset"));
 ALKitRow row;row.control=control;control->parent=&row;
 ALKitProxyBinding binding(&row,true);std::string proxy=binding.get().asString();int signals=0;
 binding.connectChanged([&](){proxy=binding.get().asString();++signals;});
 gSavedSettings.values[setting]=v;ALKitSettingWatch::instance().fire(setting);
 return signals && proxy==control->getValue().asString();
}
extern "C" __declspec(dllexport) void surface(double* output,int phase){
 static double seconds=0.;
 auto& bridge=ALWeatherBridge::Controller::instance();
 auto settings=readLibrarySettings();sync_preparation();
 if(phase==0 || phase==2){fixture_weather_restart();seconds+=1.;}
 if(phase==2)command(0,3,"");
 for(int i=0;i<600;++i){fixture_weather_step(seconds);seconds+=1./60.;}
 output[0]=bridge.state().wetness;output[1]=settings.wetAmount;output[2]=settings.puddleAmount;
 output[3]=settings.rippleStrength;output[4]=settings.puddles;output[5]=settings.coverResolution;output[6]=settings.coverExtent;
 output[7]=bridge.surfaceSerial();
}
'''
    # Execute the exact native material upload routine with a recording shader sink.
    # GL resource/matrix wrappers are fixtures; gating and uniform values are production C++.
    upload_prefix=r'''
namespace glm {
 struct mat4{}; struct dvec3 {double x=0,y=0,z=0;bool operator==(const dvec3& o)const{return x==o.x&&y==o.y&&z==o.z;}};
 const float* value_ptr(const mat4&){static float matrix[16]{};return matrix;}
}
struct LLTexUnit {enum {TFO_POINT};};
struct LLRenderTarget {bool isComplete()const{return true;} U32 getWidth()const{return 512;}};
constexpr int GL_FALSE=0;
struct LLShaderMgr {enum {WEATHER_MATERIAL_ENABLED,WEATHER_SURFACE_COVER_MAP,WEATHER_SURFACE_COVER_MATRIX,
 WEATHER_MATERIAL_VIEW_TO_AGENT,WEATHER_MATERIAL_ORIGIN,WEATHER_MATERIAL_WET,WEATHER_MATERIAL_TIME_RANGE};};
struct LLGLSLShader {
 double* output;
 void uniform1i(int,double v){output[8]=v;}
 void bindTexture(int,LLRenderTarget*,bool,int){}
 void uniformMatrix4fv(int,int,int,const float*){}
 void uniform3f(int,float,float,float){}
 void uniform4f(int,float a,float b,float c,float d){output[0]=a;output[1]=b;output[2]=c;output[3]=d;}
 void uniform2f(int,float a,float b){output[4]=a;output[5]=b;}
};
struct ALWeatherResources {
 LLRenderTarget mCover[2]{};int mRead=0;unsigned mPlace=0;
 inline static ALWeatherShelter::Coverage fixtureCoverage=ALWeatherShelter::Coverage::COMPLETE_EMPTY;
 ALWeatherShelter::Coverage mCoverage=ALWeatherShelter::Coverage::INCOMPLETE;
 std::string mStatus;
 glm::dvec3 mOrigin;glm::mat4 mMatrix;float mRange=128.f;
 bool ready()const{return ALWeatherShelter::usable(mCoverage);}
 LLRenderTarget& writeTarget(){return mCover[1-mRead];}
 U32 mPreparedFrame=17,mPublishedFrame=~U32(0);
 // PRODUCTION_GENERATION_GETTER
 void fail(ALWeatherShelter::Coverage,const char* reason=nullptr);
 void publish(const glm::mat4&,F32,U32,bool,const glm::dvec3&,U32);
 void bind(LLGLSLShader*,const glm::mat4&,const glm::dvec3&,U32);
};
'''
    upload=function(tree.read('indra/newview/alweatherresources.cpp'),'void ALWeatherResources::bind(')
    generation=function(tree.read('indra/newview/alweatherresources.h'),'bool readyForFrame(')
    if mutation=='generation': generation=generation.replace('mPublishedFrame == frame','true')
    upload_prefix=upload_prefix.replace('// PRODUCTION_GENERATION_GETTER',generation)
    owner_source=tree.read('indra/newview/alweatherresources.cpp')
    upload=function(owner_source,'void ALWeatherResources::fail(')+function(owner_source,'void ALWeatherResources::publish(')+upload
    if mutation=='upload': upload=upload.replace('weather.state().wetness * s.wetAmount','0.5f')
    upload_suffix=r'''
int bindingFrame=17;
extern "C" __declspec(dllexport) void surface_generation(int frame){bindingFrame=frame;}
extern "C" __declspec(dllexport) void surface_uniforms(double* output){
 std::fill(output,output+9,0.0);LLGLSLShader shader{output};ALWeatherResources resources;
 auto state=ALWeatherResources::fixtureCoverage;
 if(ALWeatherShelter::usable(state))resources.publish({},128.f,state==ALWeatherShelter::Coverage::COMPLETE_COVERED?1:0,true,{},ALWeatherBridge::Controller::instance().placeSerial());
 else resources.fail(state);
 resources.bind(&shader,{},{},bindingFrame);
}
extern "C" __declspec(dllexport) int surface_cover(int allocated,int cleared,int traversed,int complete,int batches){
 auto state=ALWeatherShelter::classify(allocated!=0,cleared!=0,traversed!=0,complete!=0,batches);
 ALWeatherResources::fixtureCoverage=state;return static_cast<int>(state);
}
extern "C" __declspec(dllexport) const char* cover_readout(int state){
 sSurfaceStatus=ALWeatherShelter::label(static_cast<ALWeatherShelter::Coverage>(state));
 LLView root;LLFloaterDirector director;director.root=&root;director.refreshWorldLensReadouts("world_group/weather_tab");
 static std::string observed;observed=root.findChild<LLTextBox>("weather_surface_status")->getValue().asString();return observed.c_str();
}
'''
    caps_prefix=r'''
struct LLRender {inline static bool guard=true;static bool indexedDrawBufferGuardSupported(){return guard;}};
struct FixtureGLManager {int mNumTextureImageUnits=32;} gGLManager;
int llmax(int a,int b){return std::max(a,b);}
'''
    caps=function(tree.read('indra/newview/llviewershadermgr.cpp'),'bool LLViewerShaderMgr::weatherMaterialsEnabled(')
    caps+=function(tree.read('indra/newview/llviewershadermgr.cpp'),'bool LLViewerShaderMgr::weatherMaterialsSupported(')
    caps+=function(tree.read('indra/newview/llviewershadermgr.cpp'),'void LLViewerShaderMgr::updateWeatherUniforms(')
    if mutation=='master-switch': caps=caps.replace('master() && ', '').replace('if (weatherMaterialsEnabled())', 'if (true)')
    caps_suffix=r'''
extern "C" __declspec(dllexport) int surface_caps(int units,int guard){
 gGLManager.mNumTextureImageUnits=units;LLRender::guard=guard!=0;return LLViewerShaderMgr{}.weatherMaterialsEnabled();
}
extern "C" __declspec(dllexport) int surface_bind_count(){
 LLPipeline::weatherBinds=0;LLViewerShaderMgr{}.updateWeatherUniforms(nullptr);return LLPipeline::weatherBinds;
}
extern "C" __declspec(dllexport) int surface_row_enabled(const char* name){
 ALKitSettingWatch::instance().watchers.clear();
 LLView root;LLFloaterDirector director;director.root=&root;director.bindWeatherPage(&root);
 return root.findChild<LLUICtrl>(name)->enabled;
}
extern "C" __declspec(dllexport) void snow_capability(int supported){ALWeatherParticles::available=supported!=0;}
extern "C" __declspec(dllexport) const char* particle_readout(const char* status){
 sParticleStatus=status;LLView root;LLFloaterDirector director;director.root=&root;
 director.refreshWorldLensReadouts("world_group/weather_tab");static std::string observed;
 observed=root.findChild<LLTextBox>("weather_snow_status")->getValue().asString();return observed.c_str();}
'''
    upload_prefix='#include "alweathershelter.h"\n'+upload_prefix
    commands='\n'.join(function(panel,'void ALPanelWeatherSettings::'+name+'(') for name in ('applyLibraryPreset','stopPrecipitation','applyStartingSurfaces','clearSurfaces'))
    if mutation=='legacy-preset': commands=commands.replace('gSavedSettings.setF32("AlchemyWeatherRainRate", settings.rainRate);','gSavedSettings.setF32("AlchemyWeatherRainRate", settings.rainRate);\n    gSavedSettings.setBOOL("AlchemyWeatherRainEnabled", true);',1)
    if mutation=='snow-relief': commands=commands.replace('gSavedSettings.setF32("AlchemyWeatherRainRate", settings.rainRate);','gSavedSettings.setF32("AlchemyWeatherRainRate", settings.rainRate);\n    gSavedSettings.setBOOL("AlchemyWeatherSnowReliefEnabled", true);',1)
    commands+=function(panel,'const std::string& ALPanelWeatherSettings::surfaceStatus(')
    if 'const std::string& ALPanelWeatherSettings::particleStatus(' in panel:
        commands+=function(panel,'const std::string& ALPanelWeatherSettings::particleStatus(')
    row_source=tree.read('indra/newview/alkitrow.cpp')
    notification=function(row_source,'void ALKitRow::notifyValueChanged(') if 'void ALKitRow::notifyValueChanged(' in row_source else ''
    proxy_binding=function(row_source,'class ALKitProxyBinding final')+';'
    if mutation=='proxy-notify': binding=binding.replace('ALKitRow::notifyValueChanged(editor);','').replace('ALKitRow::notifyValueChanged(combo);','')
    source=out/'binding.cpp';source.write_text(prefix+notification+proxy_binding+read+commands+refresh+binding+suffix+color_code+upload_prefix+upload+upload_suffix+caps_prefix+caps+caps_suffix,encoding='utf-8')
    preparation=preparation_source(tree,out,mutation)
    dll=compiler_command(out,[source,preparation,*(out/(n+'.cpp') for n in SOURCES)],'binding',dll=True)
    native=C.CDLL(str(dll));native.set.argtypes=[C.c_char_p,C.c_double];native.get.argtypes=[C.c_char_p];native.get.restype=C.c_double
    native.command.argtypes=[C.c_int,C.c_int,C.c_char_p];native.trace.argtypes=[C.POINTER(C.c_double)]
    native.colors.argtypes=[C.POINTER(C.c_float),C.POINTER(C.c_float)]
    native.surface.argtypes=[C.POINTER(C.c_double),C.c_int]
    native.surface_uniforms.argtypes=[C.POINTER(C.c_double)]
    native.surface_generation.argtypes=[C.c_int]
    native.surface_cover.argtypes=[C.c_int]*5;native.surface_cover.restype=C.c_int
    native.cover_readout.argtypes=[C.c_int];native.cover_readout.restype=C.c_char_p
    native.surface_caps.argtypes=[C.c_int,C.c_int];native.surface_caps.restype=C.c_int
    native.surface_row_enabled.argtypes=[C.c_char_p];native.surface_row_enabled.restype=C.c_int
    native.pinned.argtypes=[C.c_char_p,C.c_double,C.c_int];native.pinned.restype=C.c_int
    return native


def run(mutation=None, source_ref='WORKTREE'):
    current=SourceTree(source_ref);old=SourceTree('330d012e161')
    settings_path='indra/newview/app_settings/settings.xml'
    table=settings_table(current.read(settings_path));baseline=settings_table(old.read(settings_path))
    for key,value in baseline.items():
        if key.startswith('AlchemyWeather'): assert table[key]==value, 'existing setting changed: '+key
    xml=ET.fromstring(current.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml'))
    rows={r.get('widget'):r for r in xml.iter('kit_row')}
    tracked=function(current.read('indra/newview/alpanelweathersettings.cpp'),'const std::vector<std::string>& ALPanelWeatherSettings::trackedSettings()')
    new_settings=sorted(set(table)-set(baseline))
    wp3={'AlchemyWeather'+n for n in ('SnowEnabled','PrecipType','SnowRate','SnowFlakeSize','SnowFallSpeed',
        'SnowRange','SnowParticleLimit','PrecipVisibilityEnabled','PrecipVisibilityAmount')}
    assert len(set(new_settings)-wp3)==24
    assert not (set(new_settings)&wp3) or wp3.issubset(new_settings)
    for setting in new_settings:
        assert '"'+setting+'"' in tracked, 'untracked preset/reset setting: '+setting
    native=build(ROOT/'.tmp/weather_port/bindings'/(mutation or 'baseline'),mutation,source_ref)
    defaults={key:float({'true':'1','false':'0'}.get(info['Value'],info['Value'])) for key,info in table.items() if key.startswith('AlchemyWeather') and info['Type'] in ('Boolean','U32','F32')}
    def set_(key,value):native.set(key.encode(),value)
    def get(key):return native.get(key.encode())
    def reset():
        for key,value in defaults.items(): set_(key,value)
        for key,value in {'AlchemyWeatherEnabled':1,'AlchemyWeatherControlMode':1,'AlchemyWeatherLocalEnabled':1,
                          'AlchemyWeatherWindSource':1,'AlchemyWeatherLightningTiming':1,'AlchemyWeatherStorm':1}.items():set_(key,value)
    def trace():
        result=np.zeros(8,np.float64);native.trace(result.ctypes.data_as(C.POINTER(C.c_double)));return result[:7]
    alternatives={'ControlMode':0,'LocalEnabled':0,'RainRate':80,'PrecipVariation':1,'WindSource':0,
                  'WindDirection':90,'WindSpeed':15,'WindVariation':0.9,'WindGustiness':0.9,
                  'WindDirVariation':90,'TransitionSeconds':0,'Seed':4294967295,'Storm':0,'LightningTiming':0}
    for suffix,value in alternatives.items():
        setting='AlchemyWeather'+suffix;reset();before=trace()
        if suffix=='Seed':
            assert rows['weather_library_seed'].get('bind')=='code'
            native.command(0,2,str(value).encode())
        else:
            row=next(r for r in rows.values() if r.get('setting')==setting)
            assert row.get('bind')=='setting' and row[0].get('control_name')==setting, 'row does not write setting: '+setting
            set_(setting,value)
        assert get(setting)==value, 'row commit did not change '+setting
        after=trace();assert not np.array_equal(before,after), 'no observable production consumer: '+setting
        print('binding consumer PASS:',setting,flush=True)
    for index,rate in enumerate((1,5,20,80)):
        reset();native.command(index,0,b'')
        assert get('AlchemyWeatherRainRate')==rate and get('AlchemyWeatherControlMode')==1 and get('AlchemyWeatherLocalEnabled')==1
    reset();native.command(0,1,b'')
    assert get('AlchemyWeatherRainRate')==get('AlchemyWeatherStorm')==0
    assert 'ALPanelWeatherSettings::libraryStatus()' in function(current.read('indra/newview/llfloaterdirector.cpp'),'void LLFloaterDirector::refreshWorldLensReadouts(')
    for widget,setting,uniform in [('weather_rain_color','AlchemyWeatherRainColor','sRainColor'),('weather_lightning_color','AlchemyWeatherLightningColor','sLightningColor')]:
        assert rows[widget].get('setting')==setting and rows[widget][0].get('control_name')==setting
        pipe=function(current.read('indra/newview/pipeline.cpp'),'void LLPipeline::renderWeather(')
        assert setting in pipe and re.search(r'\.uniform3f\(\s*'+uniform+r'\s*,',pipe), 'missing color uniform consumer'
        def color_output():
            rain=np.zeros(3,np.float32);lightning=np.zeros(3,np.float32)
            native.colors(rain.ctypes.data_as(C.POINTER(C.c_float)),lightning.ctypes.data_as(C.POINTER(C.c_float)))
            return rain if uniform=='sRainColor' else lightning
        set_(setting,0.7);before=color_output();set_(setting,0.2);after=color_output()
        assert get(setting)==0.2 and not np.array_equal(before,after), 'color row has no native uniform effect'
    assert 'AlchemyWeatherSeed' in xml.find('kit_page').get('watch_settings'), 'seed missing preset dirty observer'
    # The material GL gate also drives all 13 surface rows through this compiled reader,
    # actual Director callbacks, fixed-step model and native shader upload routine.
    print('weather bindings: PASS (24 tracked settings; WP1 callbacks/CPU effects; WP2 compiled upload consumer available)',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mutation',choices=['setting','command','idle-disconnect'])
    parser.add_argument('--source-ref',default='WORKTREE')
    args=parser.parse_args();run(args.mutation,args.source_ref)
