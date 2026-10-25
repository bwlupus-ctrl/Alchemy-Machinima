"""Frozen Classic/New identity, switch, preset and scene gates (real WGL, no viewer build).

Invoked by relight_volumetric_gl_check.py --engines-only. References are read via
git show; production C++ dispatch/reset/preset/scene bodies are checked or compiled
into a small instrumented host. No reference shaders are copied into the fixture.
"""
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

import numpy as np

FROZEN = 'c4d3614d3e7'
NEW = '824f65e6df7'
PIPE = 'indra/newview/pipeline.cpp'
PRESETS = 'indra/newview/alprojectorshaftpresets.cpp'
SHADERS = 'indra/newview/app_settings/shaders/'


def function(src, name):
    match = re.search(r'^.*LLPipeline::' + name + r'\([^;]*?\)\s*\{', src, re.M)
    assert match, name
    return src[match.start():src.index('\n}', match.end()) + 2]


def rename(src):
    return (src.replace('gDeferredProjectorVolumetric', 'gDeferredProjectorVolumetricClassic')
            .replace('setupSpotLightVolumetric(', 'setupSpotLightVolumetricClassic(')
            .replace('renderProjectorVolumetric(', 'renderProjectorVolumetricClassic(')
            .replace('feedProjectorVolumetricBloom(', 'feedProjectorVolumetricBloomClassic('))


def compile_host(out, source):
    """Same toolchain discovery as the existing scissor trace host."""
    src = out / 'engine_host.cpp'; src.write_text(source, encoding='utf-8')
    exe = out / 'engine_host.exe'
    vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere), '-latest', '-property', 'installationPath'], text=True).strip()
    cmd = out / 'build_engine.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc "%s" /Fe:"%s" /Fo:"%s"\n' %
                   (Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat', src, exe, out / 'engine_host.obj'))
    result = subprocess.run([str(cmd)], cwd=out, capture_output=True, text=True, shell=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


def cpp_checks(V, args, check):
    """Compile production preset/scene functions, original presets, and switch wrapper.

    Only viewer API boundaries are mocked. Values are compared with the original
    c4d implementation running against its original settings.xml defaults.
    """
    tree = V.G.SourceTree('WORKTREE'); ref = V.G.SourceTree(FROZEN)
    src = tree.read(PIPE); old = ref.read(PIPE)
    for name in ['renderProjectorVolumetric', 'feedProjectorVolumetricBloom', 'setupSpotLightVolumetric']:
        check('frozen_cpp/' + name, function(src, name.replace('Volumetric', 'VolumetricClassic') if name != 'feedProjectorVolumetricBloom' else 'feedProjectorVolumetricBloomClassic') == rename(function(old, name)))
    # Keep the FIX5 whole-function pin, permitting only the explicit FIX6
    # retirement edits below. The independent reference pin is still checked;
    # every other byte of the production dispatch must remain identical.
    previous=function(V.G.SourceTree('342a2514642').read(PIPE),'renderProjectorVolumetricNew')
    reference_hash=hashlib.sha256(previous.encode()).hexdigest()
    expected=previous.replace('"BDMergeProjectorVolumetricsDefinition", 0);','"BDMergeProjectorVolumetricsDefinition", 1);')
    expected=expected.replace('// Hard always uses Full\'s adaptive steps and bypasses every cap/reservation.',
                              '// Every saved Definition uses Full; there is no admission threshold.')
    expected=expected.replace('    const glm::mat4 inverse_projection = glm::inverse(proj);\n','')
    expected=expected.replace('            F32 camera_margin = 0.f;\n','')
    expected=expected.replace('llclamp(e_max_dist,0.f,64.f), &camera_margin);','llclamp(e_max_dist,0.f,64.f));')
    a=expected.index('            const LLVector3 forward =',expected.index('const F32 budget = projvolStepBudget(quality'))
    b=expected.index('            // Remaining per-cone override',a)
    expected=expected[:a]+expected[b:]
    a=expected.index('            const F32 color_max =',a)
    b=expected.index('            const U32 cone_quality =',a)
    expected=expected[:a]+'''            // Full-only dispatch has no pose/slider admission calculation.
            const bool use_budget = false;
'''+expected[b:]
    dispatch_hash = hashlib.sha256(function(src,'renderProjectorVolumetricNew').encode()).hexdigest()
    check('new_cpp/dispatch',reference_hash == '229ba4cfd3f4bacc607e975ed8d245ff4c9c1a16b23bcc7edbae2938ec47f5df'
          and function(src,'renderProjectorVolumetricNew')==expected,sha256=dispatch_hash,reference_sha256=reference_hash)
    dispatch=function(src,'renderProjectorVolumetricNew')
    batch=dispatch[dispatch.index('for (U32 program_index ='):]
    check('new_cpp/direct_batch_MRT','march_target->bindTarget();\n        if (!halfres) glDrawBuffer(GL_COLOR_ATTACHMENT0);' in batch)
    manager = tree.read('indra/newview/llviewershadermgr.cpp')
    check('new_cpp/fallback_lifecycle',all(x in manager for x in (
        'LLGLSLShader            gDeferredProjectorVolumetricFullProgram;',
        'mShaderList.push_back(&gDeferredProjectorVolumetricFullProgram);',
        'gDeferredProjectorVolumetricFullProgram.unload();')))
    stub = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using U32=unsigned; using S32=int; using F32=float; using U64=unsigned long long;
#define LL_ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define LL_INFOS(x) std::cerr
#define LL_ENDL std::endl
struct LLSD {
    enum Kind { UNDEF, NUMBER, TEXT, MAP, ARRAY }; Kind kind=UNDEF;
    double number=0; std::string str; std::map<std::string,LLSD> map; std::vector<LLSD> array;
    LLSD()=default; LLSD(bool x):kind(NUMBER),number(x){} LLSD(int x):kind(NUMBER),number(x){}
    LLSD(unsigned x):kind(NUMBER),number(x){} LLSD(float x):kind(NUMBER),number(x){}
    LLSD(double x):kind(NUMBER),number(x){} LLSD(const char* x):kind(TEXT),str(x){}
    LLSD(const std::string& x):kind(TEXT),str(x){}
    static LLSD emptyMap(){LLSD x;x.kind=MAP;return x;} static LLSD emptyArray(){LLSD x;x.kind=ARRAY;return x;}
    bool isMap() const{return kind==MAP;} bool isString() const{return kind==TEXT;}
    bool isUndefined() const{return kind==UNDEF;} bool asBoolean() const{return number!=0;}
    int asInteger() const{return int(number);} std::string asString() const{return str;}
    bool has(const std::string& k) const{return map.count(k)!=0;}
    LLSD& operator[](const std::string& k){kind=MAP;return map[k];}
    const LLSD& operator[](const std::string& k) const{static LLSD nil;auto i=map.find(k);return i==map.end()?nil:i->second;}
    void append(LLSD x){kind=ARRAY;array.push_back(x);}
    bool operator==(const LLSD& x) const{return kind==x.kind && number==x.number && str==x.str && map==x.map && array==x.array;}
};
struct LLControlVariable {
    LLSD value,def; void resetToDefault(bool){value=def;} LLSD getDefault() const{return def;}
    LLSD get() const{return value;} void set(const LLSD& x){value=x;}
};
struct ControlPtr { LLControlVariable* p; operator LLControlVariable*() const{return p;} LLControlVariable* operator->(){return p;} LLControlVariable* get(){return p;} };
struct Settings {
    std::map<std::string,LLControlVariable> controls;
    ControlPtr getControl(const std::string& n){auto i=controls.find(n);return {i==controls.end()?nullptr:&i->second};}
    void put(const std::string& n,const LLSD& v){controls[n].value=v;}
    bool getBOOL(const std::string& n){return controls[n].value.asBoolean();}
    U32 getU32(const std::string& n){return controls[n].value.asInteger();}
    std::string getString(const std::string& n){return controls[n].value.asString();}
    void setBOOL(const std::string& n,bool v){put(n,LLSD(v));} void setU32(const std::string& n,U32 v){put(n,LLSD(v));}
    void setF32(const std::string& n,F32 v){put(n,LLSD(v));} void setString(const std::string& n,const std::string& v){put(n,LLSD(v));}
} gSavedSettings;
namespace ALPresetLook {bool replaying(){return false;} struct Load{};}
'''
    def stripped(s):
        return re.sub(r'^#include[^\n]*\n', '', s, flags=re.M)
    looks_h = stripped(tree.read('indra/newview/alprojectorshaftlooks.h'))
    looks_cpp = stripped(tree.read('indra/newview/alprojectorshaftlooks.cpp'))
    presets_h = stripped(tree.read('indra/newview/alprojectorshaftpresets.h'))
    presets_cpp = stripped(tree.read(PRESETS))
    original_h = stripped(ref.read('indra/newview/alprojectorshaftpresets.h')).replace('AL_PROJECTOR_SHAFT_PRESETS_H', 'ORIGINAL_PRESETS_H').replace('ALProjectorShaftPresets', 'Original')
    original_cpp = stripped(ref.read(PRESETS)).replace('ALProjectorShaftPresets', 'Original')
    # Run the two versions against independently loaded defaults, including Color3.
    def load_defaults(xml):
        root = ET.fromstring(xml).find('map'); statements=[]
        for i in range(0, len(root), 2):
            name = root[i].text
            if not name.startswith(('BDMerge', 'RenderSpotShadow')): continue
            node=root[i+1]; d={node[j].text:node[j+1] for j in range(0,len(node),2)}
            ty=d['Type'].text; val=d['Value']; literal=None
            if ty in ('Boolean','U32','S32','F32'):
                literal='LLSD('+str(float(val.text))+('f' if ty=='F32' else '')+')'
            elif ty=='Color3':
                statements.append('{LLSD v=LLSD::emptyArray();'+''.join('v.append(LLSD('+x.text+'f));' for x in val)+'gSavedSettings.controls["'+name+'"]={v,v};}')
            elif ty=='String': literal='LLSD('+json.dumps(val.text or '')+')'
            if literal: statements.append('gSavedSettings.controls["'+name+'"]={'+literal+','+literal+'};')
        return '\n'.join(statements)
    setup = '\nvoid oldDefaults(){gSavedSettings.controls.clear();'+load_defaults(ref.read('indra/newview/app_settings/settings.xml'))+'}\n'
    setup += '\nvoid newDefaults(){gSavedSettings.controls.clear();'+load_defaults(tree.read('indra/newview/app_settings/settings.xml'))+'}\n'
    wrapper = function(src,'renderProjectorVolumetric')
    refresh = src[src.index('    const U32 shaft_engine ='):src.index('    BDMergeProjectorVolumetrics = gSavedSettings.getBOOL')]
    switch_stub = r'''
struct LLRenderTarget {int releases=0;void release(){++releases;}};
struct {void flush(){}} gGL;
struct LLPipeline {
static U32 BDMergeProjectorVolumetricsEngine; bool mProjVolEngineDirty=false;
bool mProjVolHalfValid=true,mProjVolRimValid=true,mProjVolHistoryValid=true; LLRenderTarget* mProjVolShaftSrc=nullptr;
U32 mProjVolHistoryIdx=1;float mProjVolPrevViewProj[16]={1},mProjVolPrevModelview[16]={1};
LLRenderTarget mProjVolHalf,mProjVolRim,mProjVolHistory[2]; int mProjectorShaftCost=99; int engine=-1;
void renderProjectorVolumetric(LLRenderTarget*,bool);
void renderProjectorVolumetricClassic(LLRenderTarget*,bool){engine=0;}
void renderProjectorVolumetricNew(LLRenderTarget*,bool){engine=1;}
} gPipeline;
U32 LLPipeline::BDMergeProjectorVolumetricsEngine=1;
void refresh(){
'''+refresh.replace('BDMergeProjectorVolumetricsEngine','LLPipeline::BDMergeProjectorVolumetricsEngine').replace('"LLPipeline::BDMergeProjectorVolumetricsEngine"','"BDMergeProjectorVolumetricsEngine"')+'}\n'+wrapper
    reviewed=V.G.SourceTree('dbcec8d2b36').read(PIPE)
    reviewed_refresh=reviewed[reviewed.index('    const U32 shaft_engine ='):reviewed.index('    BDMergeProjectorVolumetrics = gSavedSettings.getBOOL')]
    switch_stub+='\nvoid reviewedRefresh(){'+reviewed_refresh.replace('BDMergeProjectorVolumetricsEngine','LLPipeline::BDMergeProjectorVolumetricsEngine').replace('"LLPipeline::BDMergeProjectorVolumetricsEngine"','"BDMergeProjectorVolumetricsEngine"')+'}\n'
    main = r'''
int main(){
newDefaults();LLPipeline::BDMergeProjectorVolumetricsEngine=1;
LLSD savedClassic=LLSD::emptyMap();savedClassic["engine"]=0;savedClassic["preset"]="2";
ALProjectorShaftPresets::applySceneData(savedClassic);reviewedRefresh();
assert(gSavedSettings.getString("BDMergeProjectorVolumetricsPreset").empty()); // negative control
LLPipeline::BDMergeProjectorVolumetricsEngine=1;
ALProjectorShaftPresets::applySceneData(savedClassic);refresh();
assert(gSavedSettings.getString("BDMergeProjectorVolumetricsPreset")=="2");
for(int p=0;p<8;++p){
 oldDefaults(); Original::apply(p); auto expected=gSavedSettings.controls;
 newDefaults();gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",0);
 assert(ALProjectorShaftPresets::apply(std::to_string(p)));
 for(const char* name:Classic::SHAFT_SETTINGS) {
  if(!(gSavedSettings.controls[name].value==expected[name].value)) std::cerr<<"preset "<<p<<" mismatch "<<name<<" current="<<gSavedSettings.controls[name].value.number<<" expected="<<expected[name].value.number<<"\n";
  assert(gSavedSettings.controls[name].value==expected[name].value);
 }
 assert(gSavedSettings.getBOOL("BDMergeProjectorVolumetricsDust")==expected["BDMergeProjectorVolumetricsDust"].value.asBoolean());
 auto scene=ALProjectorShaftPresets::sceneData();newDefaults();ALProjectorShaftPresets::applySceneData(scene);
 refresh(); // The renderer refreshes cached settings on the next frame.
 assert(gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine")==0);
 assert(gSavedSettings.getString("BDMergeProjectorVolumetricsPreset")==std::to_string(p));
 for(const char* name:Classic::SHAFT_SETTINGS) assert(gSavedSettings.controls[name].value==expected[name].value);
}
for(const auto& look:ALProjectorShaftLooks::looks()){
 newDefaults(); assert(ALProjectorShaftPresets::apply(look.id));auto saved=ALProjectorShaftPresets::sceneData();
 gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",0);ALProjectorShaftPresets::applySceneData(saved);
 refresh();
 assert(gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine")==1);
 assert(gSavedSettings.getString("BDMergeProjectorVolumetricsPreset")==look.id);
 assert(ALProjectorShaftPresets::sceneData()==saved);
}
for(int p=0;p<8;++p){
 LLSD oldScene=LLSD::emptyMap();oldScene["preset"]=std::to_string(p);
 gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",0);ALProjectorShaftPresets::applySceneData(oldScene);
 assert(gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine")==1);
 assert(gSavedSettings.getString("BDMergeProjectorVolumetricsPreset")==ALProjectorShaftLooks::canonicalLookId(std::to_string(p)));
}
gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",0);ALProjectorShaftPresets::applySceneData(LLSD());
assert(gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine")==1);
assert(ALProjectorShaftPresets::classicLabels().size()==8 && ALProjectorShaftLooks::looks().size()==13);
for(int e: {0,1,0,1,0,1}) {
 gPipeline.mProjVolHistoryValid=true; gPipeline.mProjVolHalfValid=true;gPipeline.mProjVolRimValid=true;
 gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",e);refresh();
 gPipeline.renderProjectorVolumetric(nullptr,true);
 assert(gPipeline.engine==e && !gPipeline.mProjVolHistoryValid && !gPipeline.mProjVolHalfValid && !gPipeline.mProjVolRimValid);
 assert(gPipeline.mProjVolHistoryIdx==0 && gPipeline.mProjVolPrevViewProj[0]==0 && gPipeline.mProjVolShaftSrc==nullptr);
}
assert(gPipeline.mProjVolHalf.releases==6 && gPipeline.mProjVolHistory[0].releases==6 && gPipeline.mProjVolHistory[1].releases==6);
// Multiple toggles before a render still invalidate the last rendered engine.
gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",0);refresh();
gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine",1);refresh();
gPipeline.mProjVolHistoryValid=true;gPipeline.renderProjectorVolumetric(nullptr,false);
assert(!gPipeline.mProjVolHistoryValid && gPipeline.mProjVolHalf.releases==7);
std::cout<<"presets 8/8, looks 13/13, scenes old/New/Classic, switches 7/7 PASS\n";
}
'''
    exe=compile_host(args.out,stub+looks_h+looks_cpp+presets_h+presets_cpp+original_h+original_cpp+setup+switch_stub+main)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    check('compiled_presets_scenes_switch',result.returncode==0,output=result.stdout+result.stderr)
    assert result.returncode==0,result.stdout+result.stderr


def engine_checks(V, args, gl, report):
    def check(name, passed, **details):
        report['checks'].append(dict(name=name,passed=bool(passed),**details))
        print(name,'PASS' if passed else 'FAIL',details,flush=True)

    cpp_checks(V,args,check)
    tree=V.G.SourceTree('WORKTREE')
    manager=tree.read('indra/newview/llviewershadermgr.cpp')
    baseline_manager=V.G.SourceTree(NEW).read('indra/newview/llviewershadermgr.cpp')
    def new_registration(src,end):
        return src[src.index('        gDeferredProjectorVolumetricProgram.mName ='):
                   src.index(end,src.index('        gDeferredProjectorVolumetricProgram.mName ='))].rstrip()
    expected_registration=new_registration(baseline_manager,'        // Cinematic weather').replace('''        gDeferredProjectorVolumetricProgram.addPermutation("PROJVOL_FINE_MEDIUM",
            gSavedSettings.getU32("BDMergeProjectorVolumetricsDefinition") != 1 &&
            gSavedSettings.getF32("BDMergeProjectorVolumetricsNoiseStrength") > 0.5f ? "1" : "0");
''','').replace('''gDeferredProjectorVolumetricProgram.addPermutation("PROJVOL_BUDGET_ENABLE",
            gSavedSettings.getU32("BDMergeProjectorVolumetricsDefinition") == 1 ? "0" : "1");''',
    'gDeferredProjectorVolumetricProgram.addPermutation("PROJVOL_BUDGET_ENABLE", "0");')
    check('new_shader_registration_unchanged',
          new_registration(manager,'        // Four optional Classic programs') ==
          expected_registration)
    check('unrelated_shader_cache_features_unchanged',tree.read('indra/llrender/llglslshader.h') ==
          V.G.SourceTree(NEW).read('indra/llrender/llglslshader.h'))
    check('classic_frozen_utility_registration',
          manager.count('addPermutation("PROJVOL_CLASSIC_UTIL", "1")') == 3 and
          'shader->mDefines.count("PROJVOL_CLASSIC_UTIL") == 0' in tree.read('indra/llrender/llshadermgr.cpp'))
    shader_manager=tree.read('indra/llrender/llshadermgr.cpp')
    check('classic_frozen_shadow_registration',
          'if (features->hasShadows && shader->mDefines.count("PROJVOL_CLASSIC_UTIL") == 0)' in shader_manager and
          manager.count('mShaderFiles.push_back(make_pair("deferred/projectorVolumetricClassicShadowUtil.glsl", GL_FRAGMENT_SHADER))') == 1)
    # Every frozen shader, including fallback and deferred reconstruction helper,
    # must be the exact git blob, not a normalized-text or approximate comparison.
    mapping={p:p.replace('projectorVolumetric','projectorVolumetricClassic') for p in [
        V.SHAFT,V.UP,V.BLOOM,'class1/deferred/projectorVolumetricF.glsl',
        'class1/deferred/projectorVolumetricTemporalF.glsl']}
    mapping['class1/deferred/deferredUtil.glsl']='class1/deferred/projectorVolumetricClassicUtil.glsl'
    # Both shaft engines call sampleSpotShadow: zero normals in the air,
    # receiver normals for rims. Shadow Sculpt changes the latter with detail
    # enabled, so Classic freezes the entire transitive sampler, not a new hash.
    mapping['class1/deferred/shadowUtil.glsl']='class1/deferred/projectorVolumetricClassicShadowUtil.glsl'
    for original,copy in mapping.items():
        baseline=subprocess.check_output(['git','show',FROZEN+':'+SHADERS+original],cwd=V.G.REPO)
        blob=(V.G.REPO/SHADERS/copy).read_bytes()
        check('frozen_blob/'+copy,blob==baseline,sha256=hashlib.sha256(blob).hexdigest())
    # No transitive utility used by these programs may drift unnoticed.
    for module in V.Runner.FRAGMENT_COMMON+['deferred/shadowUtil.glsl','deferred/postDeferredNoTCV.glsl']:
        if module in ('deferred/deferredUtil.glsl','deferred/shadowUtil.glsl'):continue
        level=1 if module in V.Runner.LEVEL1 or module=='deferred/postDeferredNoTCV.glsl' else 3
        path=next(SHADERS+'class%d/'%n+module for n in range(level,0,-1)
                  if V.G.SourceTree(FROZEN).exists(SHADERS+'class%d/'%n+module))
        check('shared_helper/'+module,V.G.SourceTree(FROZEN).read(path)==V.G.SourceTree('WORKTREE').read(path))

    class ClassicTree(V.G.SourceTree):
        def __init__(self): super().__init__(FROZEN)
        def read(self,path):
            relative=path.removeprefix(SHADERS)
            return (V.G.REPO/SHADERS/mapping[relative]).read_text(encoding='utf-8') if relative in mapping else V.G.SourceTree('WORKTREE').read(path)

    class SharedShadowClassicTree(ClassicTree):
        """Must-fail control: the merged surface sampler leaks detail into rims."""
        def read(self,path):
            if path == SHADERS+'class1/deferred/shadowUtil.glsl':
                return tree.read(path)
            return super().read(path)

    reference=V.Runner(V.G.SourceTree(FROZEN),gl)
    t=time.perf_counter()
    classic=V.Runner(ClassicTree(),gl)
    programs=[classic.shaft,classic.up,
              classic.link('class1/deferred/projectorVolumetricTemporalF.glsl',False),
              classic.link(V.BLOOM,False)]
    report['classic_extra_programs']=4
    report['classic_fixture_link_seconds']=time.perf_counter()-t
    lengths=[]
    for program in programs:
        length=gl.I();gl.getprog(program,0x8741,C.byref(length)) # GL_PROGRAM_BINARY_LENGTH
        lengths.append(length.value)
    report['classic_fixture_program_binary_bytes']=lengths
    # Both programs live in the same GL context, just as in the viewer. NEW is
    # pinned to the user's supplied HEAD, rather than the pre-performance round.
    before=V.Runner(V.G.SourceTree(NEW),gl); after=V.Runner(V.G.SourceTree('WORKTREE'),gl)
    shared_classic=V.Runner(SharedShadowClassicTree(),gl)
    host=V.build_scissor_host(args.out,args.glm_include)
    before.host=after.host=host
    defaults=V.settings_defaults()
    # Load the actual c4d defaults rather than the current look defaults.
    old_defaults=ET.fromstring(V.G.SourceTree(FROZEN).read('indra/newview/app_settings/settings.xml')).find('map')
    default_state={}
    for i in range(0,len(old_defaults),2):
        if not old_defaults[i].text.startswith(('BDMergeProjectorVolumetrics','BDMergeSoft','RenderSpotShadow')):
            continue
        node=old_defaults[i+1]
        fields={node[j].text:node[j+1] for j in range(0,len(node),2)}
        value=fields['Value']
        if fields['Type'].text in ('Boolean','U32','S32','F32'):
            default_state[old_defaults[i].text]=(float(value.text),)
        elif fields['Type'].text=='Color3':
            default_state[old_defaults[i].text]=tuple(float(x.text) for x in value)
    # c4d predates these integration controls: its shader implements none of
    # them. Supply off only for the shared look-to-uniform adapter.
    for suffix in ('Equiangular','AirborneTap','CookieFootprint'):
        default_state['BDMergeProjectorVolumetrics'+suffix]=(0.,)
    classic_default_spec=V.look_spec(default_state)
    classic_default_spec['kwargs'].update(
        half=bool(default_state['BDMergeProjectorVolumetricsHalfRes'][0]),
        dither=int(default_state['BDMergeProjectorVolumetricsDither'][0]),
        beam=int(default_state['BDMergeProjectorVolumetricsTemporalBeamDepth'][0]))
    new_default_spec=V.look_spec({k:v[1] for k,v in defaults.items()})
    shared_rim_changed=False
    try:
        fixtures=V.fixtures(args.width,args.height)
        for name,fixture in fixtures.items():
            # Explicit shipped Classic defaults, in addition to the existing
            # full/half/rim and dust permutations below. Flags on must also be
            # unable to change Classic's rim or airborne shadow sampling.
            _,a,_=V.render_look(reference,fixture,classic_default_spec)
            _,b,_=V.render_look(classic,fixture,classic_default_spec)
            check('classic_defaults_bytes/'+name,a.tobytes()==b.tobytes())
            _,shared,_=V.render_look(shared_classic,fixture,classic_default_spec)
            check('classic_shared_defaults_bytes/'+name,a.tobytes()==shared.tobytes())
            _,a,_=V.render_look(before,fixture,new_default_spec)
            _,b,_=V.render_look(after,fixture,new_default_spec)
            check('new_defaults_bytes/'+name,a.tobytes()==b.tobytes())
            for soft in (False,True):
                for flag in (0.,1.):
                    # GL uniforms persist between draws. Give every runner the
                    # same explicit values after the shipped-default test.
                    controls=dict(rim=1.,soft=soft,cookie=True,
                                  uniforms=dict(classic_default_spec['uniforms'],
                                                **{'spot_shadow_detail[0]':flag}))
                    _,a,_=reference.run(fixture,**controls);_,b,_=classic.run(fixture,**controls)
                    check('classic_detail_bytes/%s/soft=%s/flag=%s'%(name,soft,flag),a.tobytes()==b.tobytes())
                    _,shared,_=shared_classic.run(fixture,**controls)
                    if flag:
                        shared_rim_changed |= a.tobytes()!=shared.tobytes()
                    else:
                        check('classic_shared_flags_off/%s/soft=%s'%(name,soft),a.tobytes()==shared.tobytes())
            for half in (False,True):
                for rim in (0.,1.):
                    kwargs=dict(half=half,steps=24,rim=rim,cookie=True,beam=1,conservative=0)
                    _,a,_=reference.run(fixture,**kwargs);_,b,_=classic.run(fixture,**kwargs)
                    check('classic_bytes/%s/%s/%s'%(name,half,rim),a.tobytes()==b.tobytes())
            for look in V.parse_looks()['looks']:
                spec=V.look_spec(V.look_state(look,defaults))
                # Full is the immutable New reference. Budgeted integration has
                # dedicated before-fails/after-passes FIX1 radiance regressions.
                for hard in (False,True):
                    _,a,_=V.render_look(before,fixture,spec,hard_beam=hard)
                    _,b,_=V.render_look(after,fixture,spec,hard_beam=hard)
                    check('new_bytes/%s/%s/hard=%s'%(name,look['id'],hard),a.tobytes()==b.tobytes())
        check('classic_shared_shadow_negative_control',shared_rim_changed)
        fixture=fixtures['outside']; frames=V.micro_frames(fixture,count=9)
        # Classic temporal references include the original first-frame behavior.
        for label,runner,ref in [('Classic',classic,reference),('New',after,before)]:
            previous=np.zeros((args.height//2,args.width//2,4),np.float32)
            previous_ref=previous.copy();last=frames[0][0]
            for index,f in enumerate(frames):
                kwargs=dict(half=True,steps=24,cookie=True,beam=1,dither=1)
                runner.run(f,**kwargs);ref.run(f,**kwargs)
                a=runner.temporal_resolve(f,previous,last,index>0)
                b=ref.temporal_resolve(f,previous_ref,last,index>0)
                check('temporal_bytes/%s/%d'%(label,index),a.tobytes()==b.tobytes() and np.isfinite(a).all())
                previous,previous_ref,last=a,b,f[0]
        # Poisoned history must be ignored on every transition. Reset semantics
        # above come from the compiled production wrapper, before aux dispatch.
        for index,runner in enumerate([after,classic,after,classic,after]):
            f=frames[index]; runner.run(f,half=True,beam=1,dither=1)
            shape=(args.height//2,args.width//2,4)
            clean=runner.temporal_resolve(f,np.zeros(shape,np.float32),f[0],False)
            poisoned=runner.temporal_resolve(f,np.full(shape,np.nan,np.float32),f[0],False)
            check('switch_first_frame/%d'%index,clean.tobytes()==poisoned.tobytes() and np.isfinite(poisoned).all())
        # The original dust/conservative compile permutations remain available to
        # Classic. Exercise real 3D texture reads and both airborne shadow kernels.
        saved=V.G.PREAMBLE
        try:
            V.G.PREAMBLE+='\n#define PROJVOL_DUST_ENABLE 1\n#define PROJVOL_CONSERVATIVE_SHADOW 1\n'
            dust_ref=V.DustRunner(V.G.SourceTree(FROZEN),gl)
            dust_classic=V.DustRunner(ClassicTree(),gl)
        finally: V.G.PREAMBLE=saved
        volume,_=V.synthetic_dust_volume(gl)
        dust_ref.volume=dust_classic.volume=volume
        for name,fixture in fixtures.items():
            for half in (False,True):
                for conservative in (0,1):
                    uniforms=dict(projvol_dust=1,projvol_dust_intensity=1.6,
                                  projvol_dust_scale=.6,projvol_dust_wind=(0.,-.35,0.),
                                  projvol_noise_strength=.75,projvol_noise_scale=.35)
                    _,a,_=dust_ref.run(fixture,half=half,conservative=conservative,uniforms=uniforms)
                    _,b,_=dust_classic.run(fixture,half=half,conservative=conservative,uniforms=uniforms)
                    check('classic_dust_bytes/%s/%s/%s'%(name,half,conservative),a.tobytes()==b.tobytes())
    finally:
        host.stdin.close();assert host.wait(timeout=5)==0
    # Generation gate proves enabled_control propagates to the actual widgets.
    page=ET.parse(V.G.REPO/'indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml')
    disabled=[n for n in page.iter() if n.get('enabled_control')=='BDMergeProjectorVolumetricsEngine']
    check('kit_new_only_controls',len(disabled)==9 and all('New engine only' in n.get('tool_tip','') for n in disabled))

