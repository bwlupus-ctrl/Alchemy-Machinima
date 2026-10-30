"""Standalone real TUT plus production dispatch/identity contract mutations; no viewer build."""
from pathlib import Path
import os
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.tmp/shadow_depth_reach_20261030/math'


def contracts(pipeline,soften):
    calls=list(re.finditer(r'renderAreaShadows\(debugLightingTerm\(\).*?;',pipeline,re.S))
    assert len(calls)==2 and calls[0][0]==calls[1][0], 'two identical dispatch calls'
    assert 'if (area_early)\n        {\n            '+calls[0][0] in pipeline, 'early depth-zero guard'
    assert 'if (!area_early)\n        {\n            '+calls[1][0] in pipeline, 'late guard'
    assert 'const bool area_early = depth_amount > 0.f;' in pipeline, 'early depth-zero guard'
    assert 'const bool depth_active = area_early && areaShadowsActive();' in pipeline, 'per-view identity gate'
    assert 'depth_active ? depth_amount : 0.f' in pipeline and 'depth_active ? static_cast<F32>(llmin(mAreaDepthCount,mAreaShadowCount)) : 0.f' in pipeline, 'per-bind identity upload'
    assert 'uniform4fv(LLStaticHashedString("area_far_reach"),1,far_data.data());' in pipeline, 'reach upload'
    assert soften.count('if (area_depth.x > 0.0 && !envOnlyIdentity()) irradiance *= areaDepthFill')==2, 'real identity branch'
    assert 'if (area_depth.y > 4.0) map1 = texture(area_shadow_map1,tc);' in soften, 'plane 1 access guard'


def check():
    OUT.mkdir(exist_ok=True,parents=True)
    test=(ROOT/'indra/newview/tests/alshadowdepthmath_test.cpp').read_text()
    test=test.replace('#include "linden_common.h"','').replace('#include "../test/lltut.h"','#include <tut/tut.hpp>\n#include <tut/tut_reporter.hpp>')
    test=test.replace('#include "../alshadowdepthmath.h"','#include "alshadowdepthmath.h"')
    runner='''
namespace tut { test_runner_singleton runner; }
int main() {
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();
    return reporter.all_ok() ? 0 : 1;
}
'''
    cpp=OUT/'tut.cpp';cpp.write_text(test+runner,encoding='utf8')
    vcvars=next(Path('C:/Program Files/Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    exe=OUT/'shadow_depth_tut.exe'
    cmd=OUT/'compile.cmd'
    cmd.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /std:c++20 /EHsc /O2 /fp:fast /WX /I"{ROOT.parent / "alchemy-machinima/build-Windows-vs2026-os/vcpkg_installed/x64-windows-alchemy/include"}" /I"{ROOT / "indra/externals/tut"}" /I"{ROOT / "indra/newview"}" "{cpp}" /Fo"{OUT / "tut.obj"}" /Fe"{exe}"\nexit /b %ERRORLEVEL%\n')
    subprocess.run(['cmd','/d','/c',str(cmd)],check=True,timeout=90)
    subprocess.run([str(exe)],check=True,timeout=15)
    p=(ROOT/'indra/newview/pipeline.cpp').read_text();s=(ROOT/'indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl').read_text()
    contracts(p,s)
    # Execute both real dispatch sites and the exact per-bind upload block. The
    # policy itself is exercised over all views by relight_vcam_check; here the
    # stand-in reports whether this particular view has valid current channels.
    early=p[p.index('        static LLCachedControl<F32> shadow_depth('):p.index('        bindPrismLensTarget(*screen_target);\n        static LLCachedControl<bool> visible_diffuse')]
    late=p[p.index('        if (!area_early)',p.index('void LLPipeline::renderDeferredLighting(')):p.index('        // The diagnostic replaces opaque lighting',p.index('        if (!area_early)',p.index('void LLPipeline::renderDeferredLighting(')))]
    upload=p[p.index('            static const LLStaticHashedString depth_str("area_depth")'):p.index('            // [EnvIntensity] fallbacks aligned',p.index('            static const LLStaticHashedString depth_str("area_depth")'))]
    driver=r'''
#include "alshadowdepthmath.h"
#include <cassert>
#include <limits>
using F32=float;
float inputDepth=0;
int gSavedSettings=0,stage=0,calls=0,callStage=0;
bool active=false;
template<class T> struct LLCachedControl {
    LLCachedControl(int,const char*,T) {} T operator()() const { return inputDepth; }
};
struct LLStaticHashedString { LLStaticHashedString(const char*) {} };
struct V { float mV[4]={}; } mAreaDepthLight[8],mAreaDepthInfo[8],mAreaDepthAxis[8];
unsigned mAreaDepthCount=8,mAreaShadowCount=8;
template<class T> T llmin(T a,T b) { return a<b?a:b; }
struct Shader {
    float amount=-1,count=-1;
    void uniform2f(LLStaticHashedString,float a,float c) { amount=a;count=c; }
    void uniform4fv(LLStaticHashedString,int,const float*) {}
} soften_shader;
bool areaShadowsActive() { return active; }
int debugLightingTerm() { return 0; }
void renderAreaShadows(int,float,bool,bool,bool,bool) { ++calls;callStage=stage; }
void run() {
    const int effective_local_light_count=8;
    const float light_scale=1;
    const bool actor_only=false,actor_mask_ready=false,actor_as_world=false,actor_excluded=false;
EARLY
    stage=1;
UPLOAD
    stage=2;
LATE
}
int main() {
    for(float d : {0.f,.3f,std::numeric_limits<float>::quiet_NaN()})
        for(bool enabled : {false,true}) {
            inputDepth=d;active=enabled;calls=stage=0;run();
            const bool early=ALShadowDepth::amount(d)>0;
            assert(calls==1 && callStage==(early ? 0 : 2));
            assert(soften_shader.amount==(early && enabled ? .3f : 0.f));
            assert(soften_shader.count==(early && enabled ? 8.f : 0.f));
        }
}
'''.replace('EARLY',early).replace('UPLOAD',upload).replace('LATE',late)
    director=(ROOT/'indra/newview/llfloaterdirector.cpp').read_text()
    migration=director[director.index('    if (!scene["settings"].isMap() || !scene["settings"].has("RenderAreaShadowDepth"))'):director.index('    // [RelightQ] Old scenes retain their radius in metres')]
    compatibility=r'''
struct SceneMap {
    bool map=false,present=false;
    bool isMap() const { return map; }
    bool has(const char*) const { return present; }
};
struct Scene { SceneMap settings; SceneMap& operator[](const char*) { return settings; } };
struct DepthControl { float value=.8f; void setF32(const char*,float v) { value=v; } };
int sceneCheck() {
    for(int c : {0,1,2}) {
        Scene scene;scene.settings.map=c!=0;scene.settings.present=c==2;
        DepthControl gSavedSettings;
MIGRATION
        if(gSavedSettings.value!=(c==2 ? .8f : 0.f)) return 1;
    }
    return 0;
}
'''.replace('MIGRATION',migration)
    driver=driver.replace('int main() {',compatibility+'\nint main() {\n    if(sceneCheck()) return 1;')
    driver_cpp=OUT/'dispatch.cpp';driver_cpp.write_text(driver,encoding='utf8')
    driver_exe=OUT/'dispatch.exe';driver_cmd=OUT/'dispatch.cmd'
    driver_cmd.write_text(cmd.read_text().replace(str(cpp),str(driver_cpp)).replace(str(exe),str(driver_exe)).replace('tut.obj','dispatch.obj'))
    subprocess.run(['cmd','/d','/c',str(driver_cmd)],check=True,timeout=90)
    subprocess.run([str(driver_exe)],check=True,timeout=15)
    mutants={
        'stale_area_depth_probe_or_feed_off':(p.replace('area_early && areaShadowsActive()','area_early'),s),
        'early_call_at_depth_zero':(p.replace('const bool area_early = depth_amount > 0.f;','const bool area_early = true;'),s),
        'reach_upload_dropped':(p.replace('gAreaShadowProgram.uniform4fv(LLStaticHashedString("area_far_reach"),1,far_data.data());',''),s),
        'identity_branch_removed_nan':(p,s.replace('if (area_depth.x > 0.0 && !envOnlyIdentity()) irradiance *=','irradiance *=')),
        'plane1_read_count_le_four':(p,s.replace('if (area_depth.y > 4.0) map1 =','map1 ='))}
    for name,(mp,ms) in mutants.items():
        assert mp!=p or ms!=s,name
        try:contracts(mp,ms)
        except AssertionError as e:print('PASS source contract mutant rejected:',name,str(e),flush=True)
        else:raise AssertionError('mutant survived: '+name)
    print('PASS standalone real TUT /O2 /fp:fast /WX, executed production early/late dispatch, per-view uploads and old-scene migration, and identity contracts',flush=True)


if __name__=='__main__':check()
