"""Link real viewer shader inventories at the SSR budget boundaries; no viewer build.

The small recipe manifest records stage/feature metadata, never shader source or
expected sampler lists. Production source assembly and GL active uniforms supply
the inventories. A 32-unit GL context can model 16-unit viewer budgets without
mistaking driver link success for a safe viewer texture-channel assignment.
"""
import ctypes as C
import hashlib
import json
import subprocess
from pathlib import Path

from native_check_tools import ROOT, compile_run, check_directory
from shader_load_check import function


def run(directory):
    viewer = (ROOT / 'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    budget = function(viewer, 'ssr_texture_budget(')
    code = r'''
#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <string>
using S32=int;
struct {int mNumTextureImageUnits=32;float mGLVersion=4.6f;} gGLManager;
struct Settings{
 std::map<std::string,int> values={{"RenderReservedTextureIndices",12},{"RenderShadowDetail",2},{"RenderReflectionsEnabled",1},{"RenderMirrors",1}};
 int getS32(const std::string& s){return values[s];}bool getBOOL(const std::string& s){return values[s]!=0;}
}gSavedSettings;
struct LLViewerShaderMgr{
 bool weather=true,mRuntimeSSR=true,mRuntimeSSRLoaded=true;std::vector<S32> mSSRCapabilityInputs;
 bool weatherMaterialsSupported()const{return weather;}
 std::vector<S32> ssrCapabilityInputs()const;bool canToggleSSR()const;
};
template<class T>T llmin(T a,T b){return std::min(a,b);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
template<class T>T llclamp(T a,T b,T c){return std::clamp(a,b,c);}
'''+budget+'\n'+function(viewer, 'LLViewerShaderMgr::ssrCapabilityInputs(')+'\n'+function(viewer, 'LLViewerShaderMgr::canToggleSSR(')+r'''
int main(){
 LLViewerShaderMgr mgr;mgr.mSSRCapabilityInputs=mgr.ssrCapabilityInputs();
 if(!mgr.canToggleSSR())return 2;
 for(const auto* setting:{"RenderReservedTextureIndices","RenderShadowDetail","RenderReflectionsEnabled","RenderMirrors"}){
   auto before=gSavedSettings.values[setting];gSavedSettings.values[setting]=0;
   if(mgr.canToggleSSR())return 3;gSavedSettings.values[setting]=before;
 }
 gGLManager.mNumTextureImageUnits=16;if(mgr.canToggleSSR())return 4;gGLManager.mNumTextureImageUnits=32;
 gGLManager.mGLVersion=3.9f;if(mgr.canToggleSSR())return 5;gGLManager.mGLVersion=4.6f;
 mgr.weather=false;if(mgr.canToggleSSR())return 6;mgr.weather=true;
 for(int on:{0,1}){gSavedSettings.values["RenderScreenSpaceReflections"]=on;if(!mgr.canToggleSSR())return 7;}
 mgr.mRuntimeSSR=false;if(mgr.canToggleSSR())return 8;
 for(const std::vector<int> config:std::vector<std::vector<int>>{{32,12,1,1,1,1},{32,8,1,1,1,1},{16,12,1,1,1,1},{16,8,0,1,0,0},{32,0,0,1,1,0}}){
  auto b=ssr_texture_budget(config[0],config[1],config[2],config[3],config[4],config[5]);
  for(auto v:config)std::cout<<v<<" ";std::cout<<b.first<<" "<<b.second<<"\n";
 }
 // Feature changes can make a 16-unit GPU capable; this is not a GPU-size test.
 auto reduced=ssr_texture_budget(16,8,false,true,false,false);
 if(reduced.first<4 || reduced.second<1)return 1;
 auto shiny=ssr_texture_budget(32,0,false,true,true,false);if(shiny.first!=29)return 9;
}
'''
    result = compile_run(directory, code, name='ssr_budget', flags=['/O2', '/WX'])
    assert result.returncode == 0, result.stdout + result.stderr
    rows = [tuple(map(int, line.split())) for line in result.stdout.splitlines()]
    budgets = {row[:6]: row[6:] for row in rows}
    assert budgets == {(32, 12, 1, 1, 1, 1): (20, 4), (32, 8, 1, 1, 1, 1): (23, 4),
                       (16, 12, 1, 1, 1, 1): (-1, -1), (16, 8, 0, 1, 0, 0): (8, 2),
                       (32, 0, 0, 1, 1, 0): (29, 6)}
    # Verify that the budget drives all three production consumers.
    assert 'if (mRuntimeSSR) LLGLSLShader::sIndexedTextureChannels = ssr_budget.first;' in viewer
    assert 'if (mRuntimeSSR) LLGLSLShader::sSharedPBRIndexedGLTFChannels = ssr_budget.second;' in viewer
    assert '(mRuntimeSSR || ssr) ? 3 : 1' in viewer
    assert 'if (mRuntimeSSR) attribs["SSR_RUNTIME"] = "1";' in viewer
    assert 'else if (ssr) attribs["SSR"] = "1";' in viewer

    from shader_diagnostics_fixture import run as diagnostics
    producer = diagnostics(check_directory('shader_sampler'), assemble_only=True)
    manifest = json.loads((Path(__file__).with_name('shader_sampler_recipes.json')).read_text(encoding='utf-8'))
    recipes = manifest['programs']
    for recipe in recipes:
        recipe['attachments'] = [dict(file=f, stage=s, source_level=l) for f, s, l in recipe['attachments']]
    # Only the two modified files need baseline copies. Other stage sources are
    # identical to main and are always read from the viewer's own shader tree.
    current = ROOT / 'indra/newview/app_settings/shaders'
    baseline = directory / 'main_shaders'
    for recipe in recipes:
        for a in recipe['attachments']:
            relative = f"class{a['source_level']}/{a['file']}"
            if a['file'] == 'deferred/screenSpaceReflUtil.glsl':
                relative = 'class3/' + a['file']
            path = baseline / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            if a['file'] in ('deferred/reflectionProbeF.glsl', 'deferred/screenSpaceReflUtil.glsl'):
                data = subprocess.check_output(['git', 'show', '39aacb188c5:indra/newview/app_settings/shaders/' + relative], cwd=ROOT)
            else:
                data = (current / relative).read_bytes()
            path.write_bytes(data)
    stub = baseline / 'class1/deferred/screenSpaceReflUtil.glsl'
    stub.parent.mkdir(parents=True, exist_ok=True)
    stub.write_bytes((current / 'class1/deferred/screenSpaceReflUtil.glsl').read_bytes())
    jobs, links = {}, []
    for config, (indexed, shared) in budgets.items():
        units, reserved, shadows, probes, mirrors, weather = config
        capable = indexed >= 4
        for mode in ('main_off', 'main_on', 'unfixed_off', 'fixed_off', 'fixed_on'):
            real = mode in ('main_on', 'unfixed_off') or (mode.startswith('fixed') and (capable or mode.endswith('on')))
            for recipe in recipes:
                globals_ = dict(manifest['global_defines'])
                globals_['TERRAIN_PBR_DETAIL'] = '-1' if units == 16 else '0'
                if not shadows:
                    globals_.pop('SUN_SHADOW', None)
                    globals_.pop('SPOT_SHADOW', None)
                if not mirrors:
                    globals_.pop('HERO_PROBES', None)
                if mode == 'main_on' or (mode == 'fixed_on' and not capable):
                    globals_['SSR'] = '1'
                if mode == 'unfixed_off' or (mode.startswith('fixed') and capable):
                    globals_['SSR_RUNTIME'] = '1'
                width = indexed if mode.startswith('fixed') and capable else max(4, units - reserved)
                slots = shared if mode.startswith('fixed') and capable else max(1, min(8, (units - ((13 if mode == 'unfixed_off' else 11) if weather else 10)) // 4))
                ids = []
                for a in recipe['attachments']:
                    # These features stop attaching their modules in the
                    # reduced production recipe, as well as dropping defines.
                    if not shadows and a['file'] == 'deferred/shadowUtil.glsl':
                        continue
                    if not weather and a['file'] == 'deferred/weatherMaterialF.glsl':
                        continue
                    root = [a['file'], a['stage']] in recipe['roots']
                    defines = dict(recipe['defines']) if root else dict(globals_)
                    if not shadows:
                        defines.pop('HAS_SUN_SHADOW', None)
                    if not weather:
                        defines.pop('HAS_WEATHER_MATERIAL', None)
                    if root and 'GLTF_INDEXED_CHANNELS' in defines:
                        defines['GLTF_INDEXED_CHANNELS'] = str(slots)
                    channels = width if root and recipe['indexed'] else -1
                    level = a['source_level']
                    if a['file'] == 'deferred/screenSpaceReflUtil.glsl':
                        level = 3 if real else 1
                    tree = baseline if mode.startswith('main') else current
                    spec = (str(tree), a['file'], a['stage'], level, channels, not root,
                            mode.startswith('main'), tuple(sorted(defines.items())), tuple(sorted(globals_.items())))
                    ident = hashlib.sha256(repr(spec).encode()).hexdigest()[:24]
                    jobs[ident] = spec
                    ids.append(ident)
                links.append(dict(config=config, units=units, reserved=reserved, mode=mode, name=recipe['name'], jobs=ids))
    assembly = directory / 'sampler_assembly'
    assembly.mkdir(exist_ok=True)
    lines = []
    for ident, (tree, file, stage, level, channels, global_, legacy, defines, globals_) in jobs.items():
        words = [ident, tree.replace('\\', '/'), file, str(stage), str(level), str(channels), str(int(global_)), str(int(legacy)), str(len(defines))]
        for key, value in defines:
            words.extend((key, value))
        words.append(str(len(globals_)))
        for key, value in globals_:
            words.extend((key, value))
        lines.append(' '.join(words))
    (assembly / 'jobs.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    subprocess.run([str(producer), str(assembly), 'assemble'], check=True, cwd=ROOT)
    from relight_3d_gl import GL
    gl = GL()
    I, U = C.c_int, C.c_uint
    active = gl.fn('glGetActiveUniform', None, U, U, I, C.POINTER(I), C.POINTER(I), C.POINTER(U), C.c_void_p)
    objects, programs, results = {}, {}, []
    for recipe_index, recipe in enumerate(links):
        if recipe_index % 10 == 0:
            print(f"sampler links {recipe_index}/{len(links)}: {recipe['units']} units, {recipe['mode']}", flush=True)
        signature = tuple(recipe['jobs'])
        if signature not in programs:
            program = gl.program()
            for ident in recipe['jobs']:
                if ident not in objects:
                    obj = gl.create_shader(jobs[ident][2])
                    source = C.c_char_p((assembly / (ident + '.glsl')).read_bytes())
                    gl.source(obj, 1, C.byref(source), None)
                    gl.compile(obj)
                    ok = I()
                    gl.getshader(obj, 0x8B81, C.byref(ok))
                    log = C.create_string_buffer(65536)
                    gl.slog(obj, len(log), None, log)
                    assert ok.value, (jobs[ident][:5], log.value.decode())
                    objects[ident] = obj
                gl.attach(program, objects[ident])
            gl.link(program)
            ok = I()
            gl.getprog(program, 0x8B82, C.byref(ok))
            log = C.create_string_buffer(65536)
            gl.plog(program, len(log), None, log)
            assert ok.value, (recipe, log.value.decode())
            programs[signature] = program
        program = programs[signature]
        # Identical resident recipes use the same linked program for both
        # toggle states. Count the driver's inventory after each uniform upload.
        location = gl.loc(program, b'ssr_enabled')
        if location >= 0:
            gl.fn('glUseProgram', None, U)(program)
            gl.fn('glUniform1i', None, I, I)(location, int(recipe['mode'].endswith('on')))
            gl.fn('glUseProgram', None, U)(0)
        count, samplers = I(), {}
        gl.getprog(program, 0x8B86, C.byref(count))
        for index in range(count.value):
            name, length, size, kind = C.create_string_buffer(1024), I(), I(), U()
            active(program, index, len(name), C.byref(length), C.byref(size), C.byref(kind), name)
            if kind.value in (0x8B5E, 0x8B60, 0x8B62, 0x8DC1, 0x8DC4, 0x8DC5, 0x900C, 0x900D, 0x8B5F, 0x8DC2, 0x8DCA):
                samplers[name.value.decode()] = size.value
        results.append({k: v for k, v in recipe.items() if k != 'jobs'} | {'program': program, 'samplers': samplers, 'count': sum(samplers.values())})
    for program in programs.values():
        gl.fn('glDeleteProgram', None, U)(program)
    (directory / 'sampler_inventories.json').write_text(json.dumps({'renderer': gl.renderer, 'link_count': len(programs), 'results': results}, indent=2) + '\n', encoding='utf-8')
    def inventory(units, reserved, mode, name):
        return next(r for r in results if (r['units'], r['reserved'], r['mode'], r['name']) == (units, reserved, mode, name))
    alpha = 'Deferred Alpha Shader'
    assert inventory(32, 8, 'main_off', alpha)['count'] == 32
    assert inventory(32, 8, 'unfixed_off', alpha)['count'] == 33
    assert inventory(32, 8, 'fixed_off', alpha)['count'] == 32
    for name in ('Deferred PBR Alpha Shader', 'Shared Actor FX PBR Opaque Scalar Shader'):
        assert inventory(16, 12, 'main_off', name)['count'] == 15
        assert inventory(16, 12, 'unfixed_off', name)['count'] == 17
        assert inventory(16, 12, 'fixed_off', name)['samplers'] == inventory(16, 12, 'main_off', name)['samplers']
    # Reduced 16-unit mode and the low-reservation shiny boundary are linked,
    # including actual sampler maps and identical resident off/on programs.
    for name in ('Deferred PBR Alpha Shader', 'Shared Actor FX PBR Opaque Scalar Shader',
                 'Deferred Alpha Shader', 'Deferred Soften Shader'):
        reduced = inventory(16, 8, 'fixed_off', name)
        assert reduced['count'] <= 16, reduced
        assert not any('shadowMap' in key or 'weather_' in key or key == 'heroProbes'
                       for key in reduced['samplers']), reduced
    shiny = inventory(32, 0, 'fixed_off', 'Deferred FullbrightShiny Shader')
    assert shiny['count'] == 32, shiny
    assert sum(v for key, v in shiny['samplers'].items() if key.startswith('tex')) == 29, shiny
    for r in results:
        if r['mode'].startswith('fixed'):
            if budgets[r['config']][0] >= 4:
                assert r['count'] <= r['units'], r
                assert r['samplers'] == inventory(r['units'], r['reserved'], 'fixed_on', r['name'])['samplers']
                assert r['program'] == inventory(r['units'], r['reserved'], 'fixed_on', r['name'])['program']
            else:
                # Main's on configuration can already overflow. Preserve its
                # inventory and existing fallback behavior without adding units.
                main = 'main_on' if r['mode'] == 'fixed_on' else 'main_off'
                assert r['samplers'] == inventory(r['units'], r['reserved'], main, r['name'])['samplers'], r
    assert gl.fn('glGetError', U)() == 0
    print(f'real sampler inventories: {len(results)} inventories from {len(programs)} links; both overflow counterexamples reproduced; runtime budgets and main fallback inventories PASS')


if __name__ == '__main__':
    run(check_directory('shader_parallel'))
