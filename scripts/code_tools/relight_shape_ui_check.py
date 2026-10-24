"""Execute production slider mapping/sync and verify the generated shape rows.

--ui-ref HEAD discriminates the previous linear control and radius rounding.
The baseline adapter extracts its actual hover/thumb expressions; it does not
invent a substitute curve. No viewer or UI application is built.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]


def check(ref=None):
    def read(path):
        return (subprocess.check_output(['git','show',ref+':'+path],cwd=ROOT,text=True)
                if ref else (ROOT/path).read_text())

    text=read('indra/llui/llslider.cpp')

    def method(signature):
        start=text.index(signature); body=text.index('{',start);end=body+1;depth=1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        return text[start:end]

    methods=method('void LLSlider::setValue(F32 value,')
    cpp=read('indra/newview/llfloaterdirector.cpp')
    migration_start=cpp.find('if (!scene["settings"].has("RenderLocalLightHighlightScale"))')
    migration=''
    if migration_start>=0:
        end=cpp.index('{',migration_start)+1;depth=1
        while depth:
            depth+=(cpp[end]=='{')-(cpp[end]=='}');end+=1
        migration=cpp[migration_start:end]
    if 'F32 LLSlider::valueToFraction(' in text:
        methods+='\n'+method('F32 LLSlider::valueToFraction(')
        methods+='\n'+method('F32 LLSlider::valueFromFraction(')
        methods+='\n'+method('F32 LLSlider::steppedValue(')
    else:
        thumb=re.search(r'F32 t = ([^;]+);',method('void LLSlider::updateThumbRect()'))[1]
        hover=re.search(r'setValueAndCommit\((t[^;]+)\);',method('bool LLSlider::handleHover('))[1]
        methods+='\nF32 LLSlider::valueToFraction(F32 value) const { return '+thumb.replace('getValueF32()','value')+'; }'
        methods+='\nF32 LLSlider::valueFromFraction(F32 t) const { return '+hover+'; }'
        methods+='\nF32 LLSlider::steppedValue(F32 delta) const { return getValueF32()+delta; }'
    shim=r'''
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using F32=float;
template<class T> T llclamp(T v,T lo,T hi) { return std::clamp(v,lo,hi); }
float ll_round(float v,float increment) { return std::floor(v/increment+.5f)*increment; }
template<class T> struct LLHandle { T* p; };
namespace LLSliderWheel { template<class T> T* alive(LLHandle<T> h) { return h.p; } }
struct LLF32UICtrl { float value=0; void setValue(float v) { value=v; } };
struct LLSlider : LLF32UICtrl {
    float mMinValue=0,mMaxValue=1,mIncrement=.01f,saved=0;
    bool mQuadratic=true;
    float getValueF32() const { return value; }
    template<class T> LLHandle<T> getDerivedHandle() { return {this}; }
    void setControlValue(float v) { saved=v; }
    void updateThumbRect() {}
    void setValue(float value,bool from_event=false,bool precision_override=false);
    float valueToFraction(float value) const;
    float valueFromFraction(float fraction) const;
    float steppedValue(float delta) const;
};
int failures=0;
void require(bool ok,const char* label) { if(!ok && failures++<10) std::printf("FAIL: %s\n",label); }
struct LLControlVariable { float value=.5f; void resetToDefault(bool) { value=1; } };
struct Settings { LLControlVariable scale; LLControlVariable* getControl(const char*) { return &scale; } } gSavedSettings;
namespace ALPresetLook { void reset(const std::vector<std::string>& keys) {
    for (const auto& key:keys) gSavedSettings.getControl(key.c_str())->resetToDefault(true);
} }
struct Scene { bool hasNew; Scene& operator[](const char*) { return *this; } bool has(const char*) const { return hasNew; } };
'''
    tests=r'''
int main() {
    LLSlider s;
    for(int i=0;i<=100;++i) {
        float u=i*.01f,r=u*u;
        require(std::abs(s.valueFromFraction(u)-r)<3.e-7,"quadratic drag radius");
        require(std::abs(s.valueToFraction(r)-u)<3.e-7,"sqrt thumb restore");
        s.value=r;
        float next=std::min(1.f,u+.01f);
        require(std::abs(s.steppedValue(.01f)-next*next)<3.e-7,"quadratic wheel and keys");
    }
    for(float saved : {.0001234f,.03f,.1234567f,.9f}) {
        s.value=0;s.saved=saved;
        s.setValue(saved); // same call as the constructor
        require(std::memcmp(&s.value,&saved,sizeof(float))==0,"saved radius exact on construction");
        s.setValue(saved,true); // incoming scene/preset/control sync
        require(std::memcmp(&s.value,&saved,sizeof(float))==0,"saved radius exact on sync");
    }
    s.mQuadratic=false;s.setValue(.034f);
    require(std::abs(s.value-.03f)<1.e-7,"ordinary slider increment unchanged");
    gSavedSettings.scale.value=.5f;loadMigration(Scene{false});
    require(gSavedSettings.scale.value==1,"old scene cannot inherit highlight scale");
    gSavedSettings.scale.value=.8f;loadMigration(Scene{true});
    require(gSavedSettings.scale.value==.8f,"authored scene highlight preserved");
    std::printf("[shape-ui] %s: %d failures; u=.1 -> %.8f m\n",failures ? "FAIL" : "PASS",failures,LLSlider().valueFromFraction(.1f));
    return failures ? 1 : 0;
}
'''
    out=ROOT/'.tmp/light_shape'/('ui_before' if ref else 'ui_after');out.mkdir(parents=True,exist_ok=True)
    (out/'slider.cpp').write_text(shim+methods+'\nvoid loadMigration(Scene scene) {\n'+migration+'\n}\n'+tests)
    vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    vs=subprocess.check_output([str(vswhere),'-latest','-property','installationPath'],text=True).strip()
    cmd=out/'compile.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++17 /EHsc "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\n' %
        (Path(vs)/'VC/Auxiliary/Build/vcvars64.bat',out/'slider.cpp',out/'slider.exe',out/'slider.obj',out/'slider.exe'))
    result=subprocess.call(['cmd','/d','/c',str(cmd)],cwd=out)
    page=ET.fromstring(read('indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml'))
    slider=page.find('.//slider[@name="relight_world_size"]')
    highlight=page.find('.//slider[@name="relight_highlight_softness"]')
    preset=page.find('.//combo_box[@name="relight_shape_preset"]')
    rows_ok=(slider.get('control_name')=='RenderLocalLightDefaultRadius' and slider.get('quadratic')=='true'
             and slider.get('can_edit_text')=='true'
             and int(slider.get('decimal_digits'))>=4 and highlight is not None and preset is not None)
    if rows_ok:
        cpp=read('indra/newview/llfloaterdirector.cpp')
        radii=re.search(r'const F32 radii\[\] = \{([^}]+)\}',cpp)[1]
        scales=re.search(r'const F32 highlights\[\] = \{([^}]+)\}',cpp)[1]
        parse=lambda s:[float(v.strip().rstrip('f')) for v in s.split(',')]
        assert parse(radii)==[.03,.12,.45,.9] and parse(scales)==[1,.8,.6,.5]
        assert highlight.get('control_name')=='RenderLocalLightHighlightScale'
        assert len(preset.findall('combo_box.item'))==5
        assert cpp.count('"RenderLocalLightHighlightScale",')>=1
        rig=ET.fromstring(read('indra/newview/skins/default/xui/en/panel_cine_light_rig.xml'))
        assert rig.find('.//spinner[@name="cine_fixture_source_size"]') is not None
    print('[shape-ui] generated rows/presets', 'PASS' if rows_ok else 'FAIL',flush=True)
    return result or (0 if rows_ok else 1)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ui-ref')
    raise SystemExit(check(parser.parse_args().ui_ref))
