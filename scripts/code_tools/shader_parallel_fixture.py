"""Native compiler dispatch and real GLSL sampler budgets; no viewer build."""
import subprocess
import xml.etree.ElementTree as ET

from native_check_tools import ROOT, compile_run
from shader_load_check import function


def run(directory):
    source = (ROOT / 'indra/llrender/llgl.cpp').read_text(encoding='utf-8')
    method = function(source, 'LLGLManager::enableParallelShaderCompile(')
    code = r'''
#include <cstdint>
#include <cassert>
#include <set>
#include <string>
#include <vector>
#include <iostream>
#define APIENTRY
#define LL_DARWIN 0
using GLuint=unsigned;
int hints=0;GLuint threads=0;
std::uintptr_t khr=0,arb=0;
std::vector<std::string> lookups;
void hint(GLuint n){++hints;threads=n;}
using Proc=void(*)();
Proc get_proc(const char* name){
 lookups.emplace_back(name);
 return reinterpret_cast<Proc>(std::string(name).ends_with("KHR")?khr:arb);
}
#define LL_GET_PROC_ADDRESS(name) get_proc(name)
struct LLGLManager{
 std::set<std::string> mGLExtensions;
 bool mParallelShaderCompilerConfigured=false;
 bool enableParallelShaderCompile();
};
'''
    code += method + r'''
int main(){
 auto reset=[](){hints=0;threads=0;khr=arb=0;lookups.clear();};
 reset();LLGLManager absent;
 assert(!absent.enableParallelShaderCompile() && lookups.empty() && hints==0);
 for(std::uintptr_t missing:std::vector<std::uintptr_t>{0,1,2,3,static_cast<std::uintptr_t>(-1)}){
   reset();LLGLManager manager;manager.mGLExtensions.insert("GL_KHR_parallel_shader_compile");
   khr=missing;assert(!manager.enableParallelShaderCompile() && hints==0);
 }
 for(bool useKHR:{false,true}){
   reset();LLGLManager manager;
   manager.mGLExtensions.insert(useKHR?"GL_KHR_parallel_shader_compile":"GL_ARB_parallel_shader_compile");
   khr=arb=reinterpret_cast<std::uintptr_t>(&hint);
   assert(manager.enableParallelShaderCompile() && hints==1 && threads==0xFFFFFFFFu);
   auto count=lookups.size();assert(manager.enableParallelShaderCompile());
   assert(hints==1 && lookups.size()==count);
   manager.mParallelShaderCompilerConfigured=false; // A replacement GL context.
   assert(manager.enableParallelShaderCompile() && hints==2);
 }
 reset();LLGLManager both;
 both.mGLExtensions={"GL_KHR_parallel_shader_compile","GL_ARB_parallel_shader_compile"};
 arb=reinterpret_cast<std::uintptr_t>(&hint);
 assert(both.enableParallelShaderCompile() && hints==1 && lookups.size()==2);
 reset();both.mParallelShaderCompilerConfigured=false;khr=arb=reinterpret_cast<std::uintptr_t>(&hint);
 assert(both.enableParallelShaderCompile() && lookups.size()==1);
 std::cout<<"parallel capability: absent extension, missing/sentinel entry, KHR/ARB, preference, fallback and once per context PASS\n";
}
'''
    result = compile_run(directory, code, name='parallel_capability', flags=['/O2', '/WX'])
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, end='')

    settings = list(ET.parse(ROOT / 'indra/newview/app_settings/settings.xml').getroot().find('map'))
    entries = [v for k, v in zip(settings[::2], settings[1::2]) if k.text == 'RenderParallelShaderCompile']
    assert len(entries) == 1
    fields = list(entries[0])
    metadata = {k.text: v.text for k, v in zip(fields[::2], fields[1::2])}
    assert (metadata['Type'], metadata['Persist'], metadata['Value']) == ('Boolean', '1', '0')
    viewer = (ROOT / 'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    original = subprocess.check_output(['git', 'show', 'HEAD:indra/newview/llviewershadermgr.cpp'],
                                       cwd=ROOT, text=True, encoding='utf-8')
    expected_basic = function(original, 'LLViewerShaderMgr::loadBasicShaders(')
    expected_basic = expected_basic.replace('    bool ssr = gSavedSettings.getBOOL("RenderScreenSpaceReflections");\n\n', '')
    expected_basic = expected_basic.replace('    if (ssr)\n    {\n        attribs["SSR"] = "1";\n    }\n\n', '')
    expected_basic = expected_basic.replace('ssr ? 3 : 1', '(mRuntimeSSR || ssr) ? 3 : 1')
    expected_basic = expected_basic.replace('    std::map<std::string, std::string> attribs;\n', '    std::map<std::string, std::string> attribs;\n    const bool ssr = gSavedSettings.getBOOL("RenderScreenSpaceReflections");\n    if (mRuntimeSSR) attribs["SSR_RUNTIME"] = "1";\n    else if (ssr) attribs["SSR"] = "1";\n')
    expected_basic = expected_basic.replace('GL_VERTEX_SHADER, &attribs)', 'GL_VERTEX_SHADER, &LLGLSLShader::sGlobalDefines)')
    expected_basic = expected_basic.replace('GL_FRAGMENT_SHADER, &attribs, index_channels[i])', 'GL_FRAGMENT_SHADER, &LLGLSLShader::sGlobalDefines, index_channels[i])')
    expected_basic = expected_basic.replace("    // binary-cache hash covers it (llglslshader.cpp), and the utility files'\n    // defines loop emits only this token -- so no compile unit ever sees a\n    // duplicate GBUFFER_AVATAR_TAG.", "    // binary-cache hash covers it explicitly (llglslshader.cpp). The hash\n    // token itself is not emitted, so no compile unit ever sees a duplicate\n    // GBUFFER_AVATAR_TAG.")
    expected_basic = expected_basic.replace('    // Load basic dependency shaders first', '    mRuntimeSSRLoaded = false;\n    bool runtime_ssr_loaded = mRuntimeSSR;\n    // Load basic dependency shaders first')
    expected_basic = expected_basic.replace('        // Note usage of GL_FRAGMENT_SHADER\n', '        // Note usage of GL_FRAGMENT_SHADER\n        const S32 requested_level = shaders[i].second;\n')
    expected_basic = expected_basic.replace('            return shaders[i].first;\n        }\n    }\n\n    // Snapshot', '''            return shaders[i].first;
        }
        // A successful load may have selected a lower class, including the
        // no-SSR stub. Only the real, requested modules permit a runtime toggle.
        if ((shaders[i].first == "deferred/screenSpaceReflUtil.glsl" ||
             shaders[i].first == "deferred/reflectionProbeF.glsl") &&
            (shaders[i].second != requested_level || mLastShaderSourceLevel != requested_level))
        {
            runtime_ssr_loaded = false;
        }
    }

    // Snapshot''')
    expected_basic = expected_basic.replace('    // Snapshot only preloaded modules', '    mRuntimeSSRLoaded = runtime_ssr_loaded;\n\n    // Snapshot only preloaded modules')
    assert function(viewer, 'LLViewerShaderMgr::loadBasicShaders(') == expected_basic
    deferred = function(viewer, 'LLViewerShaderMgr::loadShadersDeferred(')
    assert 'parallel_requested && gGLManager.enableParallelShaderCompile()' in deferred
    assert deferred.count('load_family(') == 14
    for shader in ('gDeferredMultiLightProgram[i]', 'gDeferredSpotLightProgram',
                   'gDeferredMultiSpotLightProgram', 'gDeferredAlphaProgram'):
        assert shader in deferred
    # These serial sampler/level barriers must not disappear during conversion.
    for expression in ('mActiveTextureChannels > gGLManager.mNumTextureImageUnits',
                       'gRigRimAuxCompiled = false;', 'disable_indexed_gltf_batching();'):
        assert expression in deferred
    assert 'SHARED_FORWARD_PBR_AUX_TEXTURE_UNITS = weatherMaterialsSupported() ? 11 : 10;' in viewer
    from shader_sampler_fixture import run as sampler_inventories
    sampler_inventories(directory)
    print('parallel setting and serial basic/sampler/eligibility barriers PASS')


if __name__ == '__main__':
    from native_check_tools import check_directory
    run(check_directory('shader_diagnostics'))
