"""Execute production radius/shape and status code with small CPU objects.

Return the actual float radii to the production sphere/disk GPU fixtures.
No viewer build; HEAD functions provide an independent OFF byte comparison.
"""
from pathlib import Path
import json
import os
import subprocess
import xml.etree.ElementTree as ET
from relight_area_policy_check import method


def check_prim_size():
    nv = Path('indra/newview')
    pipeline = (nv / 'pipeline.cpp').read_text(encoding='utf-8')
    assert 'lights[i].set(centre.x,centre.y,centre.z,area_source_radius(volume))' in pipeline
    assert 'const F32 radius = area_source_radius(volume, &reference)' in pipeline
    scene = method(nv / 'llfloaterdirector.cpp',
                   'const std::vector<std::string>& LLFloaterDirector::sceneSettingsList()')
    assert scene.count('"RenderLocalLightSizeFromPrim"') == 1
    # The generated row must name a real switch, at the requested location.
    page = ET.parse(nv / 'skins/default/xui/en/panel_dpage_light_rim.xml')
    rows = page.findall('.//kit_card[@id="light_shape"]/kit_row')
    widgets = [row.get('widget') for row in rows]
    row = rows[widgets.index('relight_world_size') + 1]
    assert row.get('widget') == 'relight_size_from_prim'
    assert row.find('kit_switch').get('control_name') == 'RenderLocalLightSizeFromPrim'
    assert all(term in row.get('keywords') for term in ('prim size', 'light size', 'bulb size'))
    default = page.find('.//text[@name="area_shadow_sl_status"]')
    assert default.text == 'SL shadows are off in Preferences; Soft shadows is off'
    directory = Path('.tmp/relight_step3_source').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    baseline = subprocess.check_output(['git', 'show', 'HEAD:indra/newview/pipeline.cpp'], text=True)
    (directory / 'baseline.cpp').write_text(baseline, encoding='utf-8')
    fixture = r'''
#include "alrelightmath.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using F32 = float;
using S32 = int;
struct LLVector3 { float mV[3]; };
struct LLVector4 {
    float mV[4];
    LLVector4(float x=0,float y=0,float z=0,float w=0) : mV{x,y,z,w} {}
};
struct LLVOVolume {
    int id; LLVector3 scale; mutable int scaleReads = 0;
    int getID() const { return id; }
    const LLVector3& getScale() const { ++scaleReads; return scale; }
    float getLightRadius() const { return 10; }
    float getLightFalloff(float) const { return .5f; }
};
struct Settings {
    std::map<std::string,double> values;
    int getS32(const char* k) const { return static_cast<int>(values.at(k)); }
    bool getBOOL(const char* k) const { return values.at(k)!=0; }
} gSavedSettings;
template<class T> struct LLCachedControl {
    const char* key;
    LLCachedControl(Settings&,const char* k,T) : key(k) {}
    T operator()() const { return static_cast<T>(gSavedSettings.values.at(key)); }
};
template<class T> T llclamp(T v,T lo,T hi) { return std::clamp(v,lo,hi); }
struct ALCineLightRigManager {
    std::map<int,LLVector4> sources;
    static ALCineLightRigManager& instance() { static ALCineLightRigManager m; return m; }
    bool lightShapeSourceFor(int id,LLVector4& source) const {
        auto it=sources.find(id); if(it==sources.end()) return false;
        source=it->second; return true;
    }
};
bool shapeEnabled = true;
bool light_shape_enabled() { return shapeEnabled; }
constexpr float DEFERRED_LIGHT_FALLOFF = .5f;
#define LL_DEBUGS(x) std::ostringstream()
#define LL_ENDL std::endl
'''
    for path, suffix in [(nv / 'pipeline.cpp', ''), (directory / 'baseline.cpp', '_baseline')]:
        for signature in ('static F32 area_source_radius(', 'static LLVector4 light_shape_for_volume('):
            body = method(path, signature)
            if suffix:
                body = body.replace('area_source_radius(', 'area_source_radius_baseline(')
                body = body.replace('light_shape_for_volume(', 'light_shape_for_volume_baseline(')
            fixture += '\n' + body
    director = method(nv / 'llfloaterdirector.cpp', 'void LLFloaterDirector::refreshLauncherCards()')
    status = director[director.index('        const S32 detail'):director.index('\n    }')]
    fixture += r'''
struct LLTextBox { std::string text; bool visible = true; };
void setTextIfChanged(LLTextBox* s,const char* t) { s->text=t; }
struct ALKitRow { static void setControlVisible(LLTextBox* s,bool v) { s->visible=v; } };
void statusRefresh(LLTextBox* status) {
'''+status+'\n}\n'
    fixture += r'''
int main() {
    auto& settings=gSavedSettings.values;
    settings={{"RenderLocalLightDefaultRadius",.37}, {"RenderLocalLightSizeFromPrim",0},
        {"RenderLocalLightNearFloor",.1}, {"RenderLocalLightPhysicalMaxBoost",4},
        {"RenderLocalLightPhysicalFalloff",0}, {"RenderAreaShadows",0}, {"RenderShadowDetail",0}};
    // All use the emitting object's scale. The child root/rotation/attachment
    // transform deliberately has no part in the radius API.
    LLVOVolume lights[]={{1,{{.02f,.04f,.06f}}}, {2,{{8,4,6}}},
        {3,{{4,3,.02f}}}, {4,{{.8f,.6f,.4f}}}, {5,{{.12f,.3f,.2f}}},
        {6,{{.9f,.9f,.9f}}}, {7,{{.01f,.01f,.01f}}}, {8,{{8,8,8}}}};
    auto& sources=ALCineLightRigManager::instance().sources;
    sources[6]=LLVector4(1.6f,3,0,0); // authored fixture may exceed world 1 m clamp
    sources[7]=sources[6]; // mirror keeps copied fixture metadata, not clone scale
    sources[8]=LLVector4(-1,3,0,0); // legacy/catchlight global fallback
    for (double global : {0.,.37,1.,8.}) for (bool physical : {false,true}) {
        settings["RenderLocalLightDefaultRadius"]=global;
        settings["RenderLocalLightPhysicalFalloff"]=physical;
        settings["RenderLocalLightSizeFromPrim"]=0;
        for (auto& v : lights) {
            const float a=area_source_radius(&v), b=area_source_radius_baseline(&v);
            assert(std::memcmp(&a,&b,sizeof(a))==0);
            const auto x=light_shape_for_volume(&v), y=light_shape_for_volume_baseline(&v);
            assert(std::memcmp(x.mV,y.mV,sizeof(x.mV))==0);
            assert(v.scaleReads==0); // OFF never inspects prim dimensions
        }
    }
    settings["RenderLocalLightDefaultRadius"]=.37;
    settings["RenderLocalLightPhysicalFalloff"]=0;
    settings["RenderLocalLightSizeFromPrim"]=1;
    float expected[]={.01f,1,.01f,.2f,.06f,1.6f,1.6f,.37f};
    const char* names[]={"tiny","housing","panel","child","attachment","fixture","mirror","catchlight"};
    std::cout << "{";
    for (int i=0;i<8;++i) {
        auto& v=lights[i];
        const float radius=area_source_radius(&v);
        assert(radius==expected[i]);
        const auto shape=light_shape_for_volume(&v);
        assert(shape.mV[0]==radius && shape.mV[3]==-1);
        // Live edits are read immediately; rotation never supplies a world AABB.
        const auto original=v.scale;
        v.scale={{2,2,2}};
        assert(area_source_radius(&v)==(i<5 ? 1.f : expected[i]));
        v.scale=original;
        shapeEnabled=false;
        const auto point=light_shape_for_volume(&v);
        assert(point.mV[0]==0 && area_source_radius(&v)==radius); // area independent
        shapeEnabled=true;
        std::cout << (i ? "," : "") << "\"" << names[i] << "\":" << radius;
    }
    std::cout << "}";
    for (int detail : {0,1,2,3}) for (bool soft : {false,true}) {
        settings["RenderShadowDetail"]=detail; settings["RenderAreaShadows"]=soft;
        LLTextBox status; statusRefresh(&status);
        assert(status.visible==(detail<2));
        const std::string prefix=detail==0 ? "SL shadows" : "SL projector shadows";
        assert(status.text==prefix+" are off in Preferences; Soft shadows is "+(soft ? "on" : "off"));
    }
}
'''
    (directory / 'source.cpp').write_text(fixture, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory / 'compile.cmd').write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /I"{nv.resolve()}" '
        f'/Fo"{directory / "source.obj"}" /Fe"{directory / "source.exe"}" '
        f'"{directory / "source.cpp"}"\nexit /b %ERRORLEVEL%\n', encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(directory / 'compile.cmd')], check=True)
    radii = json.loads(subprocess.check_output([str(directory / 'source.exe')], text=True))
    print('[RelightQ] prim source CPU PASS: emitting scale, live edit, rig/mirror precedence, '
          'independent area, HEAD OFF bytes; Preferences detail 0/1/2/3 x Soft shadows OFF/ON', flush=True)
    return radii


if __name__ == '__main__':
    print(check_prim_size())
