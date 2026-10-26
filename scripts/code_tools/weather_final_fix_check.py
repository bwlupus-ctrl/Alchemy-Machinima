"""Final weather regressions: production scene loops, wind and visible roof policy.

Compile extracted production C++ with small settings/geometry service adapters.
--reference never changes HEAD and demonstrates the pre-fix failures.
"""
import argparse
import os
os.environ['PYTHONUTF8'] = '1'
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET
from relight_3d_gl import SourceTree
from weather_compute_fixture import function
from weather_cpu_check import ROOT, compiler_command, prepare


def check(ref, item):
    tree = SourceTree(ref)
    director = tree.read('indra/newview/llfloaterdirector.cpp')
    pipe = tree.read('indra/newview/pipeline.cpp')
    header = tree.read('indra/newview/alweathershelter.h')
    out = ROOT / '.tmp/weather_port/final_fix' / ref / item
    prepare(out)
    if item == 'scene':
        panel = tree.read('indra/newview/alpanelweathersettings.cpp')
        tracked = function(panel, 'const std::vector<std::string>& ALPanelWeatherSettings::trackedSettings()')
        excluded = {'AlchemyWeatherLightningSeed', 'AlchemyWeatherShelterDebug'}
        keys = sorted(set(re.findall(r'"(AlchemyWeather\w+)"', tracked)) - excluded)
        listing = function(director, 'const std::vector<std::string>& LLFloaterDirector::sceneSettingsList()')
        start = director.index('    LLSD settings = LLSD::emptyMap();', director.index('LLSD LLFloaterDirector::captureSceneData()'))
        save = director[start:director.index('    scene["projector_shafts"]', start)]
        load = function(function(director, 'bool LLFloaterDirector::loadScene('), 'if (scene["settings"].isMap())')
        recovery = function(director, 'void LLFloaterDirector::initializeSceneRecovery()')
        assert 'sceneSettingsList()' in recovery and 'control->getSignal()->connect(' in recovery
        assert '++recoveryServiceState().mSettingsRevision' in recovery, 'weather dirty tracking disconnected'
        code = r'''
#include "alweatheruimodel.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <cassert>
struct LLSD {double value=0; bool map=false;std::map<std::string,LLSD> entries;
 LLSD()=default;LLSD(double v):value(v){}static LLSD emptyMap(){LLSD v;v.map=true;return v;}
 LLSD& operator[](const std::string& k){map=true;return entries[k];}
 bool isMap()const{return map;}bool has(const std::string& k)const{return entries.count(k);}
 int asInteger()const{return int(value);}};
struct LLControlVariable {LLSD v;LLSD getValue(){return v;}void setValue(LLSD a){v=a;}};
struct Settings {std::map<std::string,LLControlVariable> values;
 LLControlVariable* getControl(const std::string& k){return &values[k];}} gSavedSettings;
namespace ALCineLightRigPersistence {int legacyFXForStorage(int v){return v;}}
namespace ALProjectorShaftPresets {std::vector<std::string> trackedSettings(){return {};}}
namespace ALRotoInk {const std::vector<std::string>& settings(){static std::vector<std::string> v;return v;}}
namespace ALTron {const std::vector<std::string>& settings(){static std::vector<std::string> v;return v;}}
struct LLFloaterDirector {static const std::vector<std::string>& sceneSettingsList();};
const std::vector<std::string>& sceneSettingsList(){return LLFloaterDirector::sceneSettingsList();}
'''
        model = tree.read('indra/newview/alweatheruimodel.cpp')
        code += '\n'.join(function(model, signature) for signature in (
            'F32 ALWeatherUIModel::presetRate(', 'const char* ALWeatherUIModel::presetName(',
            'void ALWeatherUIModel::applyPreset('))
        code += listing + '\nLLSD saveScene(){LLSD scene;' + save + 'return scene;}\n'
        code += 'void loadScene(LLSD scene){' + load + '}\n'
        code += 'int main(){std::vector<std::string> keys={' + ','.join('"'+k+'"' for k in keys) + '};\n'
        code += r'''
 // A Downpour look, then clear every weather control, then restore the scene.
 for(unsigned i=0;i<keys.size();++i)gSavedSettings.values[keys[i]].v=LLSD(i+1.25);
 ALWeatherBridge::Settings look;
 ALWeatherUIModel::applyPreset(look,3);
 assert(std::string(ALWeatherUIModel::presetName(3))=="Downpour");
 gSavedSettings.values["AlchemyWeatherEnabled"].v=LLSD(1);
 gSavedSettings.values["AlchemyWeatherControlMode"].v=LLSD(look.mode);
 gSavedSettings.values["AlchemyWeatherLocalEnabled"].v=LLSD(look.local);
 gSavedSettings.values["AlchemyWeatherRainRate"].v=LLSD(look.rainRate);
 gSavedSettings.values["AlchemyWeatherPrecipType"].v=LLSD(look.precipType);
 gSavedSettings.values["AlchemyWeatherSnowEnabled"].v=LLSD(look.snowEnabled);
 const auto saved=saveScene();
 for(const auto& k:keys){assert(saved.entries.at("settings").has(k));gSavedSettings.values[k].v=LLSD(0);}
 loadScene(saved);
 for(const auto& k:keys)assert(gSavedSettings.values[k].v.value==saved.entries.at("settings").entries.at(k).value);
 LLSD old;old["settings"]=LLSD::emptyMap();loadScene(old);
 for(const auto& k:keys)assert(gSavedSettings.values[k].v.value==saved.entries.at("settings").entries.at(k).value);
 for(const auto& k:keys){gSavedSettings.values[k].v.value+=1;
  assert(saveScene().entries.at("settings").entries.at(k).value!=saved.entries.at("settings").entries.at(k).value);}
'''
        for key in excluded:
            code += 'assert(!saved.entries.at("settings").has("' + key + '"));\n'
        code += '}\n'
    elif item == 'wind':
        rain = function(pipe, 'void LLPipeline::renderWeather(')
        shelter = function(pipe, 'void LLPipeline::generateWeatherAdvancedShelter(')
        assert 'ALWeatherShelter::effectiveWind(' in rain and 'ALWeatherShelter::effectiveWind(' in shelter, 'shelter/streak shared effective wind missing'
        source = tree.read('indra/newview/alweathershelter.cpp')
        code = '#include <array>\n#include <algorithm>\n#include <cmath>\n#include <cassert>\nusing F32=float;\nnamespace ALWeatherShelter {\n'
        code += function(source, 'std::array<F32, 2> effectiveWind(') + '\n'
        code += function(source, 'std::array<F32, 2> fallShear(') + '\n}\n'
        start = shelter.index('    static LLCachedControl<F32> wind_scale_setting')
        block = shelter[start:shelter.index('    // Stable angular', start)]
        code += r'''
float gSavedSettings=0;
template<class T>struct LLCachedControl {float& value;LLCachedControl(float& v,const char*,T):value(v){}T operator()()const{return T(value);}};
struct Weather {struct State {float windMean[2]{14,-7};};State state(){return {}; }bool active(){return true;}};
struct Settings {bool snowEnabled=false,shelterTilt=true;unsigned precipType=0;float snowSpeed=2,shelterFallSpeed=28;};
std::array<F32,2> shelterWind(bool snow){Weather weather;Settings s;s.snowEnabled=snow;s.precipType=snow?1:0;
'''
        code += block + 'return shear;}\n'
        code += r'''
int main(){for(float scale:{0.f,1.f}){
 auto wind=ALWeatherShelter::effectiveWind(14,-7,scale);
 auto shear=ALWeatherShelter::fallShear(wind[0],wind[1],28,true);
 assert(wind[0]==14*scale && wind[1]==-7*scale);
 assert(shear[0]==.5f*scale && shear[1]==-.25f*scale);
}assert(ALWeatherShelter::effectiveWind(NAN,INFINITY,NAN)[0]==0);
 assert(shelterWind(false)[0]==0);assert(shelterWind(true)[0]>0);}
'''
    elif item == 'visible':
        volume = tree.read('indra/newview/llvovolume.cpp')
        assert 'ALWeatherShelter::VisibleSurfaceRay visible_surface_ray;' in pipe, 'shelter ray still uses picking semantics'
        assert 'if (!ALWeatherShelter::VisibleSurfaceRay::active()) pick_transparent |= isHiglightedOrBeacon();' in volume, 'beacons alter weather ray'
        assert '&& specialHoverCursor()' in volume and '!ALWeatherShelter::VisibleSurfaceRay::active()' in volume, 'scripted cursor alters weather ray'
        assert 'ALWeatherShelter::visibleSurface(' in volume and 'getID() == IMG_INVISIBLE' in volume, 'invisible roof filter missing'
        source = tree.read('indra/newview/alweathershelter.cpp')
        policy = function(source, 'bool visibleSurface(')
        scope = function(header, 'class VisibleSurfaceRay') + ';'
        code = '#include <cassert>\n#include <cmath>\n#include <initializer_list>\nusing F32=float;\nnamespace ALWeatherShelter {\n' + scope + '\n' + policy + '\n}\n'
        # Compile the entire production volume intersection, including beacon,
        # script-cursor, per-face alpha, texture-mask and nearest-hit behavior.
        code += r'''
#include <cstring>
using S32=int;using U8=unsigned char;
struct LLVector4Logical {void clear(){}template<int>void setElement(){}};
struct LLVector4a {float v[4]{};const float* getF32ptr()const{return v;}
 void load3(const float* p){std::memcpy(v,p,12);}void normalize3fast(){}
 void setSelectWithMask(LLVector4Logical,const LLVector4a& p,const LLVector4a&){*this=p;}};
struct LLVector3 {float mV[3];explicit LLVector3(const float* p){std::memcpy(mV,p,12);}};
struct LLVector2 {};
struct Color {float mV[4]{1,1,1,1};};
struct LLMaterial {enum {DIFFUSE_ALPHA_MODE_EMISSIVE,DIFFUSE_ALPHA_MODE_NONE,DIFFUSE_ALPHA_MODE_MASK};
 U8 getDiffuseAlphaMode(){return DIFFUSE_ALPHA_MODE_NONE;}int getAlphaMaskCutoff(){return 0;}};
constexpr int IMG_INVISIBLE=42,CLICK_ACTION_IGNORE=7;
struct LLTextureEntry {Color color;int id=0;Color getColor()const{return color;}int getID()const{return id;}
 LLMaterial* getMaterialParams()const{return nullptr;}};
struct Texture {bool hasGLTexture(){return true;}bool getMask(LLVector2){return true;}};
struct LLFace {LLTextureEntry entry;Texture texture;const LLTextureEntry* getTextureEntry(){return &entry;}
 Texture* getTexture(){return &texture;}LLVector2 surfaceToTexture(LLVector2 a,LLVector4a,LLVector4a){return a;}};
struct LLDrawable {enum {RIGGED};LLFace faces[2];int count=1;bool isDead(){return false;}
 int getRenderType(){return 0;}bool isState(int){return false;}int getNumFaces(){return count;}
 LLFace* getFace(int i){return &faces[i];}};
struct Pipeline {bool hasRenderType(int){return true;}}gPipeline;
struct LLVolume {int count=1;int getNumVolumeFaces(){return count;}
 int lineSegmentIntersect(LLVector4a,LLVector4a,int i,LLVector4a* p,LLVector2*,LLVector4a*,LLVector4a*){
  p->v[2]=4.f+i*4;return i;}};
struct Probe {bool isNull(){return true;}};
struct LLFloater {static bool isVisible(int){return false;}};int gFloaterTools=0;
struct LLSelectMgr {static LLSelectMgr& instance(){static LLSelectMgr s;return s;}
 template<class T>bool canSelectObject(T*,bool){return true;}};
struct LLToolMgr {static LLToolMgr* getInstance(){static LLToolMgr s;return &s;}bool inBuildMode(){return true;}};
struct LLRiggedVolume {enum {DO_NOT_UPDATE_FACES};};
struct LLVOVolume {bool mbCanSelect=true,beacon=false,script=false;int click=0;
 LLDrawable drawable;LLDrawable* mDrawable=&drawable;LLVolume volume;LLVolume* mRiggedVolume=&volume;Probe mReflectionProbe;
 int getClickAction(){return click;}LLVolume* getVolume(){volume.count=drawable.count;return &volume;}
 bool isHiglightedOrBeacon(){return beacon;}bool specialHoverCursor(){return script;}
 const LLTextureEntry* getTE(int i){return &drawable.faces[i].entry;}
 void updateRiggedVolume(bool,int,bool=true){}
 LLVector3 agentPositionToVolume(LLVector3 p){return p;}LLVector3 volumePositionToAgent(LLVector3 p){return p;}
 LLVector3 volumeDirectionToAgent(LLVector3 p){return p;}
 bool lineSegmentIntersect(const LLVector4a&,const LLVector4a&,S32,bool,bool,bool,S32*,LLVector4a*,LLVector2*,LLVector4a*,LLVector4a*);
};
'''
        code += function(volume, 'bool LLVOVolume::lineSegmentIntersect(')
        code += r'''
int main(){using namespace ALWeatherShelter;
 for(bool beacon:{false,true})for(bool script:{false,true}){
  LLVOVolume object;object.beacon=beacon;object.script=script;
  LLVector4a start,end,hit;int face=-1;
  auto cast=[&](){return object.lineSegmentIntersect(start,end,-1,false,false,true,&face,&hit,nullptr,nullptr,nullptr);};
  assert(!VisibleSurfaceRay::active());
  {VisibleSurfaceRay ray;assert(VisibleSurfaceRay::active());
   object.drawable.faces[0].entry.color.mV[3]=0;assert(!cast());
   object.drawable.faces[0].entry.color.mV[3]=1;
   object.drawable.faces[0].entry.id=IMG_INVISIBLE;assert(!cast());
   object.drawable.faces[0].entry.id=0;assert(cast());
   // Click-through, unselectable opaque surfaces still stop weather.
   object.click=CLICK_ACTION_IGNORE;object.mbCanSelect=false;assert(cast());
   // An invisible lower face must not conceal the opaque roof above it.
   object.drawable.faces[0].entry.color.mV[3]=0;object.drawable.count=2;
   assert(cast() && face==1);
   {VisibleSurfaceRay nested;assert(VisibleSurfaceRay::active());}
   assert(VisibleSurfaceRay::active());}
  assert(!VisibleSurfaceRay::active());
  object.mbCanSelect=true;object.click=0;object.drawable.count=1;
  object.drawable.faces[0].entry.id=0;object.drawable.faces[0].entry.color.mV[3]=0;
  assert(cast()==(beacon || script)); // Existing editor picking still reveals it.
 }}
'''
    elif item == 'ui':
        page = ET.fromstring(tree.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml'))
        rows = [r for r in page.iter('kit_row') if r.get('setting') == 'AlchemyWeatherShelterFade']
        assert len(rows) == 1 and rows[0].get('label') == 'Rain shelter', 'searchable Rain shelter toggle missing'
        assert 'rain' in rows[0].get('keywords','')
        return
    elif item == 'whitespace':
        source = tree.read('scripts/code_tools/weather_shelter_gl_check.py')
        assert source.endswith('\n') and not source.endswith('\n\n'), 'trailing blank line'
        return
    else:
        raise ValueError(item)
    path = out / 'regression.cpp'
    path.write_text(code, encoding='utf-8')
    exe = compiler_command(out, [path], 'regression')
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, item + ': ' + result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', default='WORKTREE')
    parser.add_argument('--item', choices=['scene','wind','visible','ui','whitespace'])
    args = parser.parse_args()
    failed = []
    for item in ([args.item] if args.item else ['scene','wind','visible','ui','whitespace']):
        try:
            check(args.reference, item)
            print('PASS', item, flush=True)
        except (AssertionError, ValueError) as error:
            failed.append(item)
            print('FAIL', item, str(error), flush=True)
    raise SystemExit(bool(failed))


if __name__ == '__main__':
    main()
