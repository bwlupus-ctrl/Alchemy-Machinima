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


def settings_table(text):
    entries = list(ET.fromstring(text).find('map'))
    return {entries[i].text: {**{children[j].text: children[j+1].text for j in range(0,len(children),2)},
                             'FullXML': ET.tostring(entries[i+1], encoding='unicode')}
            for i in range(0,len(entries),2) if entries[i].tag=='key'
            and entries[i+1].tag=='map'
            for children in [list(entries[i+1]) ]}


def build(out, mutation=None):
    prepare(out)
    panel=(ROOT/'indra/newview/alpanelweathersettings.cpp').read_text(encoding='utf-8')
    director=(ROOT/'indra/newview/llfloaterdirector.cpp').read_text(encoding='utf-8')
    read=function(panel,'ALWeatherBridge::Settings readLibrarySettings()')
    binding=function(director,'void LLFloaterDirector::bindWeatherPage(')
    refresh=function(director,'void LLFloaterDirector::refreshWorldLensReadouts(')
    pipe=function((ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8'),'void LLPipeline::renderWeather(')
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
struct LLUICtrl {
 LLSD value;std::function<void(LLUICtrl*,const LLSD&)> callback;
 virtual ~LLUICtrl()=default;
 void setValue(const LLSD& v){value=v;}LLSD getValue()const{return value;}
 void setCommitCallback(std::function<void(LLUICtrl*,const LLSD&)> cb){callback=cb;}
 template<class T> LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
 void fire(){if(callback)callback(this,{});}
};
struct LLButton:LLUICtrl{};struct LLComboBox:LLUICtrl{};struct LLLineEditor:LLUICtrl{};struct LLTextBox:LLUICtrl{};
struct LLView {std::map<std::string,std::unique_ptr<LLUICtrl>> widgets;
 template<class T> T* findChild(const char* name){auto& p=widgets[name];if(!p)p=std::make_unique<T>();return dynamic_cast<T*>(p.get());}
};
struct ALKitSettingWatch {static ALKitSettingWatch& instance(){static ALKitSettingWatch s;return s;}
 template<class F>int watch(const char*,F){return 1;}};
struct Signal {template<class F>int connect(F){return 1;}};
ALWeatherUIModel sLibraryUI;
struct ALPanelWeatherSettings {
 static void applyLibraryPreset(U32);static void stopPrecipitation();
 static void strikeLightningNow(){}static void confirmResetAll(){}
 static Signal& libraryChanged(){static Signal s;return s;}
 static const std::string& libraryStatus(){return sLibraryUI.status();}
};
struct LLPipeline {static constexpr bool sRenderDeferred=true;};
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
extern "C" __declspec(dllexport) void set(const char* key,double value){gSavedSettings.values[key]=value;}
extern "C" __declspec(dllexport) double get(const char* key){return gSavedSettings.values[key];}
extern "C" __declspec(dllexport) void command(int preset,int action,const char* text){
 LLView root; LLFloaterDirector director;director.root=&root;director.bindWeatherPage(&root);
 if(action==0){root.findChild<LLComboBox>("weather_library_preset")->setValue(preset);root.findChild<LLButton>("weather_library_apply")->fire();}
 if(action==1)root.findChild<LLButton>("weather_library_stop")->fire();
 if(action==2){auto* seed=root.findChild<LLLineEditor>("weather_library_seed");seed->setValue(text);seed->fire();}
}
extern "C" __declspec(dllexport) void trace(double* output){
 auto settings=readLibrarySettings();
 ALWeatherBridge::Controller bridge;ALWeatherModel::Config lightning;lightning.mEnabled=true;lightning.mRatePerMinute=60;
 std::fill(output,output+8,0.0);
 for(U64 frame=0;frame<=7200;++frame){
  bridge.update(frame,frame/60.0,settings,1.f,2.f,1,lightning,{});
  const auto r=ALWeatherBridge::rain(bridge.state(),0.55f,0.65f);
  output[0]+=r.intensity;output[1]+=r.density;output[2]+=r.windX;output[3]+=r.windY;
  output[4]+=bridge.lightning().mFlash;output[5]+=bridge.lightning().mStrikeDistanceMeters;
  output[6]+=bridge.state().rainFall;
 }
 sLibraryUI.refresh(settings,bridge.state(),bridge.catchingUp());output[7]=sLibraryUI.revision();
}
'''
    commands='\n'.join(function(panel,'void ALPanelWeatherSettings::'+name+'(') for name in ('applyLibraryPreset','stopPrecipitation'))
    source=out/'binding.cpp';source.write_text(prefix+read+commands+refresh+binding+suffix+color_code,encoding='utf-8')
    dll=compiler_command(out,[source,*(ROOT/('indra/newview/'+n+'.cpp') for n in SOURCES)],'binding',dll=True)
    native=C.CDLL(str(dll));native.set.argtypes=[C.c_char_p,C.c_double];native.get.argtypes=[C.c_char_p];native.get.restype=C.c_double
    native.command.argtypes=[C.c_int,C.c_int,C.c_char_p];native.trace.argtypes=[C.POINTER(C.c_double)]
    native.colors.argtypes=[C.POINTER(C.c_float),C.POINTER(C.c_float)]
    return native


def run(mutation=None):
    current=SourceTree('WORKTREE');old=SourceTree('330d012e161')
    settings_path='indra/newview/app_settings/settings.xml'
    table=settings_table(current.read(settings_path));baseline=settings_table(old.read(settings_path))
    for key,value in baseline.items():
        if key.startswith('AlchemyWeather'): assert table[key]==value, 'existing setting changed: '+key
    xml=ET.fromstring(current.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml'))
    rows={r.get('widget'):r for r in xml.iter('kit_row')}
    tracked=function(current.read('indra/newview/alpanelweathersettings.cpp'),'const std::vector<std::string>& ALPanelWeatherSettings::trackedSettings()')
    new_settings=sorted(set(table)-set(baseline))
    assert len(new_settings)==14
    for setting in new_settings:
        assert '"'+setting+'"' in tracked, 'untracked preset/reset setting: '+setting
    native=build(ROOT/'.tmp/weather_port/bindings'/(mutation or 'baseline'),mutation)
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
    print('weather bindings: PASS (20 new rows, 14 settings, real command callbacks and CPU effects)',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--mutation',choices=['setting','command']);args=parser.parse_args();run(args.mutation)
