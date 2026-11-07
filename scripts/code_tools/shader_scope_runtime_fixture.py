"""Execute the production SSR binder/listener and check the exact shader migration."""
from pathlib import Path
import subprocess
from native_check_tools import ROOT, compile_run, check_directory
from shader_load_check import function


def run(directory=None):
    directory = directory or check_directory('shader_scope_runtime')
    pipeline = (ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    control = (ROOT/'indra/newview/llviewercontrol.cpp').read_text(encoding='utf-8')
    binder = function(pipeline, 'LLPipeline::bindReflectionProbes(')
    handler = function(control, 'handleScreenSpaceReflectionsChanged(')
    oldhandler = function(control, 'handleReflectionProbeDetailChanged(')
    assert handler == oldhandler.replace('handleReflectionProbeDetailChanged', 'handleScreenSpaceReflectionsChanged').replace('        LLViewerShaderMgr::instance()->setShaders();', '        if (!LLViewerShaderMgr::instance()->canToggleSSR())\n        {\n            LLViewerShaderMgr::instance()->setShaders();\n        }')
    assert '"RenderScreenSpaceReflections", handleScreenSpaceReflectionsChanged' in control
    for name in ('RenderMirrors', 'RenderReflectionsEnabled', 'RenderReflectionProbeLevel', 'RenderReflectionProbeDetail', 'RenderProbeCaptureScale'):
        assert '"'+name+'", handleReflectionProbeDetailChanged' in control
    shader_path = 'indra/newview/app_settings/shaders/class3/deferred/reflectionProbeF.glsl'
    old = subprocess.check_output(['git','show','39aacb188c5:'+shader_path],cwd=ROOT,text=True,encoding='utf-8')
    new = (ROOT/shader_path).read_text(encoding='utf-8')
    expected = old.replace('#if defined(SSR)', '#if defined(SSR) || defined(SSR_RUNTIME)')
    expected = expected.replace('uniform int cube_snapshot;', '#ifdef SSR_RUNTIME\nuniform int ssr_enabled;\n#endif\nuniform int cube_snapshot;')
    expected = expected.replace('if (prism_auxiliary == 0 && cube_snapshot != 1', 'if (\n#ifdef SSR_RUNTIME\n        ssr_enabled != 0 &&\n#endif\n        prism_auxiliary == 0 && cube_snapshot != 1')
    assert new == expected, 'SSR shader changed beyond the uniform gate'
    helper_path = 'indra/newview/app_settings/shaders/class3/deferred/screenSpaceReflUtil.glsl'
    old_helper = subprocess.check_output(['git','show','39aacb188c5:'+helper_path],cwd=ROOT,text=True,encoding='utf-8')
    helper = (ROOT/helper_path).read_text(encoding='utf-8')
    gate = """
#ifdef SSR_RUNTIME
    // Preserve the class1 no-SSR stub for direct callers as well as probes.
    // The post program is resident even when SSR is disabled.
    if (ssr_enabled == 0)
    {
        collectedColor = vec4(0);
        return 0.f;
    }
#endif
"""
    signature = 'float tapScreenSpaceReflection(int totalSamples, vec2 tc, vec3 viewPos, vec3 n, inout vec4 collectedColor, sampler2D source, float glossiness)\n{'
    assert helper == old_helper.replace('uniform sampler2D sceneMap;', '#ifdef SSR_RUNTIME\nuniform int ssr_enabled;\n#endif\nuniform sampler2D sceneMap;', 1).replace(signature, signature + gate)

    preview = (ROOT/'indra/newview/llgltfmaterialpreviewmgr.cpp').read_text(encoding='utf-8')
    assert 'no_ssr(&LLPipeline::RenderScreenSpaceReflections, false)' in preview
    assert 'shader.uniform1i(ssr_enabled, RenderScreenSpaceReflections ? 1 : 0);' in pipeline
    print('SSR shader: exact old trace/mix bodies and old guards preserved; off skips both bodies, on executes old conditions; preview override and mirror/probe handlers retained PASS')
    import re
    constants = sorted(set(re.findall(r'LLShaderMgr::(\w+)', binder)))
    prefix = r'''
#include <cassert>
#include <string>
#include <map>
#include <iostream>
using S32=int;using U32=unsigned;using F32=float;using GLfloat=float;
struct LLSD{};
struct LLViewerShaderMgr{
 bool runtime=true;int reloads=0;
 static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}
 bool canToggleSSR(){return runtime;}void setShaders(){++reloads;}
};
struct LLStaticHashedString{std::string value;LLStaticHashedString(const char* s):value(s){}};
struct Target{bool complete=false;bool isComplete(){return complete;}int getDepth(){return complete?9:0;}};
struct Texture{};Texture black;
struct LLViewerTexture{inline static Texture* sBlackImagep=&black;};
struct Unit{int binding=777;bool bind(Target* target,bool depth=false){assert(target->complete);binding=depth?9:8;return true;}
 bool bind(Texture* texture){assert(texture==&black);binding=1;return true;} } units[16];
struct Renderer{Unit* getTexUnit(int channel){assert(channel>=0&&channel<16);return &units[channel];}}gGL;
struct ProbeTexture{void bind(int){};};
struct ProbePtr{bool notNull(){return false;}ProbeTexture* operator->(){static ProbeTexture t;return &t;}};
struct Probes{ProbePtr mTexture,mIrradianceMaps;int mUBO=0,resetCount=0,refreshCount=0;
 void setUniforms(){}void reset(){++resetCount;}void refreshSettings(){++refreshCount;}};
struct LLShaderMgr{ENUMS};
struct LLGLSLShader{std::map<std::string,int> values;
 void uniform1i(const LLStaticHashedString& name,int value){values[name.value]=value;}
 void uniform1f(int,float){}
 int enableTexture(int name,int=0){return name==LLShaderMgr::SCENE_MAP?1:(name==LLShaderMgr::SCENE_DEPTH?2:-1);}};
struct LLTexUnit{static constexpr int TT_CUBE_MAP_ARRAY=0;};
bool setting=false;
struct LLPipeline{
 inline static bool RenderScreenSpaceReflections=false,sPrismLensRender=false,sReflectionProbesEnabled=true,RenderMirrors=true;
 inline static int RenderScreenSpaceReflectionIterations=16,RenderScreenSpaceReflectionGlossySamples=4;
 inline static float RenderScreenSpaceReflectionDistanceBias=0,RenderScreenSpaceReflectionRayStep=0,RenderScreenSpaceReflectionDepthRejectBias=0,RenderScreenSpaceReflectionAdaptiveStepMultiplier=0;
 Target mSceneMap;Probes mReflectionMapManager,mHeroProbeManager;
 bool mPrismSavedProbeDataValid=false;int mPoissonOffset=0,releases=0,creates=0;
 bool isInit(){return true;}static void refreshCachedSettings(){RenderScreenSpaceReflections=setting;}
 void releaseGLBuffers(){++releases;mSceneMap.complete=false;}
 void createGLBuffers(){++creates;mSceneMap.complete=RenderScreenSpaceReflections;}
 void setEnvMat(LLGLSLShader&){}void bindReflectionProbes(LLGLSLShader&);
}gPipeline;
'''.replace('ENUMS', 'enum{'+','.join(constants)+'};')
    code = prefix + binder + '\n' + handler + r'''
int main(){
 LLGLSLShader shader;
 for(bool on:{true,false,true,false}){
   setting=on;handleScreenSpaceReflectionsChanged({});
   assert(gPipeline.mSceneMap.complete==on);
   units[1].binding=units[2].binding=777;gPipeline.bindReflectionProbes(shader);
   assert(shader.values["ssr_enabled"]==static_cast<int>(on));
   assert(units[1].binding==(on?8:1) && units[2].binding==(on?9:1));
 }
 assert(gPipeline.releases==4 && gPipeline.creates==4);
 assert(gPipeline.mReflectionMapManager.resetCount==4 && gPipeline.mHeroProbeManager.resetCount==4 && gPipeline.mReflectionMapManager.refreshCount==4);
 auto* mgr=LLViewerShaderMgr::instance();assert(mgr->reloads==0);
 mgr->runtime=false;
 for(bool on:{true,false,true,false}){setting=on;handleScreenSpaceReflectionsChanged({});}
 assert(mgr->reloads==4);mgr->runtime=true;
 // Auxiliary view and failed allocation never touch a released/main target.
 LLPipeline::RenderScreenSpaceReflections=true;LLPipeline::sPrismLensRender=true;
 gPipeline.bindReflectionProbes(shader);assert(units[1].binding==1 && units[2].binding==1);
 LLPipeline::sPrismLensRender=false;gPipeline.mSceneMap.complete=false;
 gPipeline.bindReflectionProbes(shader);assert(units[1].binding==1 && units[2].binding==1);
 LLPipeline::sReflectionProbesEnabled=false;LLPipeline::RenderScreenSpaceReflections=false;
 units[1].binding=units[2].binding=777;gPipeline.bindReflectionProbes(shader);
 assert(units[1].binding==1 && units[2].binding==1);
 // Preview's temporary pipeline override wins over the saved setting.
 setting=true;LLPipeline::RenderScreenSpaceReflections=false;gPipeline.bindReflectionProbes(shader);
 assert(shader.values["ssr_enabled"]==0 && units[1].binding==1 && units[2].binding==1);
 std::cout<<"production SSR listener/binder: repeated on/off, complete target vs black colour+depth, stale binding replacement, auxiliary, allocation failure, preview override, probe resets and buffer reallocation PASS\n";
}
'''
    result = compile_run(directory, code, name='ssr_runtime', flags=['/O2','/WX'])
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout,end='')

if __name__=='__main__':
    run()
