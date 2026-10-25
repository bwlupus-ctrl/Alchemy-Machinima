"""Local-light EV must change radiance, never soft-shadow coverage or shape.

Compiles the production area selection loop, planner, admission/Thin weights,
UUID channel lookup and EV factor with small viewer adapters. Its results drive
the real WGL gather/filter/world-cube resolve. No viewer build or checkout.
--source-ref accepts a commit for a failing-before run (default WORKTREE).
The committed 4f1c152fa08 defect is also a mandatory negative control.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import subprocess

import numpy as np
import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_cube_source as CS
import relight_world_gl_check as W

BASE = '4f1c152fa0890f16c53939ab8fd649555223973f'
EVS = (0, .2, .39, .4, 1, 2, -1)


def function(text, signature):
    start = text.index(signature)
    begin = text.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


SHIM = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "alareashadowmath.h"
#include "alworldshadowmath.h"
using F32=float; using F64=double; using S32=int; using U32=unsigned; using U8=unsigned char; using LLUUID=int;
template<class T> T llclamp(T x,T lo,T hi) { return std::clamp(x,lo,hi); }
struct LLColor3 {
    float mV[3];
    LLColor3(float x=0):mV{x,x,x} {}
    LLColor3 operator*(float s) const { LLColor3 c; for(int i=0;i<3;++i)c.mV[i]=mV[i]*s; return c; }
    float magVecSquared() const { return mV[0]*mV[0]+mV[1]*mV[1]+mV[2]*mV[2]; }
};
struct LLVector3 {
    float mV[3]={0,0,0};
    LLVector3 operator-(const LLVector3& b)const { LLVector3 v;for(int i=0;i<3;++i)v.mV[i]=mV[i]-b.mV[i];return v; }
    float length()const {return std::sqrt((*this)*(*this));}
    float operator*(const LLVector3& b)const {return mV[0]*b.mV[0]+mV[1]*b.mV[1]+mV[2]*b.mV[2];}
};
struct LLVector4 { float mV[4]={0,0,1,-1}; };
struct LLVector4a { void load3(const float*){} void splat(float){} };
struct LLViewerObject { bool rig=false; bool isCineRigEmitter()const{return rig;} };
struct LLVOVolume:LLViewerObject {
    int id=0; float colour=.8f; bool projector=false;
    int getID()const{return id;} bool isHUDAttachment()const{return false;}
    float getLightRadius()const{return 10.f;} LLColor3 getLightLinearColor()const{return LLColor3(colour);}
};
struct LLDrawable {
    static const int LIGHT=1; LLVOVolume* volume=nullptr;
    LLVOVolume* getVOVolume()const{return volume;} bool isState(int)const{return true;}
    LLVector3 getPositionAgent()const{return {{0,0,2}};}
};
float ev=0; bool includeRig=false;
struct Controls {} gSavedSettings;
template<class T> struct LLCachedControl {
    std::string name; T fallback;
    LLCachedControl(Controls&,const char* n,T v):name(n),fallback(v){}
    operator T()const {if(name=="AlchemyEnvLocalLightEV")return T(ev);if(name=="AlchemyEnvLocalLightIncludeRig")return T(includeRig);return fallback;}
    T operator()()const{return T(*this);}
};
namespace ALEnvIntensity { bool isNeutral(){return false;} constexpr float EV_CLAMP=6.f;
EV_CLAMP_FUNCTION
EV_FUNCTION
}
SELECTION_HELPER
struct Registry {
    bool isMirrorLight(int)const{return false;} bool areaShadowSuppressed(const LLVOVolume*)const{return false;}
    U8 actorOnlyBits(int)const{return 0;} U8 liveRecipientBits()const{return 255;}
};
struct Camera {
    bool AABBInFrustumNoFarClip(const LLVector4a&,const LLVector4a&)const{return true;}
    LLVector3 getOrigin()const{return {};} LLVector3 getAtAxis()const{return {{0,0,1}};}
};
struct LLPrismLens { static bool projectorRenderParams(const LLVOVolume* v,LLVector3&){return v->projector;} };
bool bdmerge_should_render_projector(){return true;}
bool world_shadow_eligible(LLDrawable*){return true;}
struct Pipeline {
    static const unsigned MAX_SPOT_SHADOWS=5;
    std::vector<LLDrawable*> mLights;
    LLDrawable* mShadowSpotLight[MAX_SPOT_SHADOWS]={}; float mSpotLightFade[MAX_SPOT_SHADOWS]={1,1,1,1,1};
    ALWorldShadow::Residency<int> mWorldShadowResident;
    ALAreaShadow::Selection<int> candidates;
    ALAreaShadow::Ranking<int,ALWorldShadow::kMaxPicks> picks;
    bool picked=false, owned=false;
    bool spotShadowMapSampled(unsigned)const{return true;}
    bool isWorldShadowPicked(int)const{return picked;}
    bool worldShadowOwned(int,unsigned&)const{return owned;}
    ALAreaShadow::Selection<int> select(bool cube_main) {
        Registry registry; Camera camera; const float light_scale=1;
        const bool actor_only=false,actor_as_world=false,gSnapshot=false;
        const auto toggles=[](LLVOVolume*){return true;};
        const auto actually_drawn=[](LLDrawable*,U8){return true;};
        const auto cap=[](){return 4;};
        LLVector4 selection_clip; selection_clip.mV[2]=2; selection_clip.mV[3]=20;
        ALAreaShadow::Selection<LLUUID> selected,world_candidates;
        ALAreaShadow::Ranking<LLUUID,ALWorldShadow::kMaxPicks> world_picked;
SELECTION_LOOP
        candidates=world_candidates;picks=world_picked;
        return selected;
    }
    bool areaShadowsActive()const{return true;}
    unsigned mAreaShadowCount=0; int mAreaShadowIDs[8]={};
    LLVector4 deferredLightShape(const LLVOVolume*)const;
};
LLVector4 light_shape_for_volume(const LLVOVolume*){return {};}
CHANNEL_FUNCTION
bool deferred_colour(const LLVOVolume* volume,float& radiance) {
    const float light_scale=1;
DEFERRED_COLOUR
    radiance=col.mV[0];return true;
}
void probe_copy3(float* out,const float* in){std::copy(in,in+3,out);}
bool probe_colour(const LLVOVolume* light,float& radiance) {
    const float light_scale=1;
    struct {float mRadius=10,mColor[3]={};} s;
PROBE_COLOUR
    bool eligible=true;
PROBE_ELIGIBILITY
    radiance=s.mColor[0];return eligible;
}
int main() {
    std::puts("["); bool first=true;
    for(int path=0;path<4;++path)for(int targetRig=0;targetRig<2;++targetRig)for(int incl=0;incl<2;++incl)
    for(int competition=0;competition<2;++competition)for(int thin=0;thin<2;++thin)for(int level=0;level<3;++level)
    for(float stop:{EV_SWEEP}) {
        ev=stop;includeRig=incl;
        const bool projector=path%2, cube=path>=2;
        LLVOVolume v[5]; LLDrawable d[5]; Pipeline p;
        for(int i=0;i<5;++i){v[i].id=i;v[i].projector=projector;d[i].volume=&v[i];}
        for(int i:{0,3,4}){v[i].colour=8;v[i].rig=targetRig;}
        v[1].colour=level==0 ? .8f:(level==1 ? .02f:.01f);v[1].rig=targetRig;
        // Boundary between .39 and .40 EV, with the opposite rig inclusion.
        v[2].colour=v[1].colour*std::exp2(targetRig ? -.395f:.395f);v[2].rig=!targetRig;
        // Five lights contest the ordinary four-channel cap, at the default
        // 1500 M budget. Three leaders leave one slot at the EV boundary.
        p.mLights={&d[1]};if(competition)p.mLights={&d[0],&d[1],&d[2],&d[3],&d[4]};
        if(projector)for(int i=0;i<5;++i)p.mShadowSpotLight[i]=&d[i];
        // A picked native projector transfers to its rendered cube; a point
        // cube resident holds its channel ahead of score-ranked lights.
        p.picked=p.owned=cube;
        if(cube){p.mWorldShadowResident.held[0]=true;p.mWorldShadowResident.id[0]=1;}
        auto selected=p.select(cube);
        bool native=false;for(unsigned i=0;i<selected.size;++i)if(selected.lights[i].id==1)native=selected.lights[i].shadowMap;
        auto uniform=ALAreaShadow::quality(3);
        auto requested=thin ? ALAreaShadow::segmented(uniform,3):uniform;
        auto work=ALAreaShadow::workloadOrUniform(requested,uniform,1920,1080,selected.size,1500,4,0);
        const unsigned wanted_lights=selected.size;
ADMISSION
        p.mAreaShadowCount=selected.size;for(unsigned i=0;i<selected.size;++i)p.mAreaShadowIDs[i]=selected.lights[i].id;
        auto shape=p.deferredLightShape(&v[1]);
        unsigned slot=unsigned(shape.mV[3]);
        double strength=slot<selected.size ? admission[slot]:0;
        double regular=slot<selected.size ? regular_mix[slot]:0;
        // renderAreaShadows overrides admission dropout for a rendered cube.
        if(cube && !native && slot<selected.size)strength=1;
        unsigned bits=0;float radiance=0;bool drawn=deferred_colour(&v[1],radiance);std::memcpy(&bits,&radiance,4);
        float probe_radiance=0;bool probe_eligible=probe_colour(&v[1],probe_radiance);
        std::printf("%s{\"path\":%d,\"rig\":%d,\"include\":%d,\"competition\":%d,\"thin\":%d,\"level\":%d,\"ev\":%.9g,\"slot\":%.9g,\"strength\":%.17g,\"regular\":%.17g,\"drawn\":%d,\"native\":%d,\"radiance\":%.9g,\"radiance_bits\":%u,\"plan\":[%d,%d,%d,%d,%d],\"ids\":[",
            first?"":",",path,targetRig,incl,competition,thin,level,double(stop),double(shape.mV[3]),strength,regular,drawn,native,double(radiance),bits,
            work.q.samples,work.q.steps,work.q.half,work.q.nearSteps,work.rest.steps);
        for(unsigned i=0;i<selected.size;++i)std::printf("%s%d",i?",":"",selected.lights[i].id);
        std::printf("],\"scores\":[");for(unsigned i=0;i<selected.size;++i)std::printf("%s%.17g",i?",":"",selected.lights[i].score);
        std::printf("],\"world_rank\":[");for(unsigned i=0;i<p.candidates.size;++i)std::printf("%s%d",i?",":"",p.candidates.lights[i].id);
        std::printf("],\"picked_rank\":[");for(unsigned i=0;i<p.picks.size;++i)std::printf("%s%d",i?",":"",p.picks.lights[i].id);
        std::printf("],\"probe_eligible\":%d,\"probe_radiance\":%.9g}",probe_eligible,double(probe_radiance));first=false;
    }
    std::puts("]");
}
'''


def compile_selection(ref, out):
    out.mkdir(parents=True, exist_ok=True)
    tree = G.SourceTree(ref)
    pipe = tree.read('indra/newview/pipeline.cpp')
    env = tree.read('indra/newview/alenvintensity.cpp')
    render = function(pipe, 'void LLPipeline::renderAreaShadows(')
    assert 'dropout.mV[i] = 0.f;' in render[render.index('if (cube_main && !selected.lights[plane*4+i].shadowMap'):], 'world owner must bypass admission dropout'
    start = render.index('    for (LLDrawable* drawable : mLights)', render.index('const bool pick_record_pass'))
    loop = function(render[start:], '    for (LLDrawable* drawable : mLights)')
    admission = render[render.index('    std::array<double,8> admission;'):render.index('    if (!sPrismLensRender)', render.index('    std::array<double,8> admission;'))]
    helper = function(pipe, 'static LLColor3 local_light_selection_color(') if 'static LLColor3 local_light_selection_color(' in pipe else ''
    source = SHIM.replace('EV_CLAMP_FUNCTION', function(env, 'inline F32 clampEV('))
    source = source.replace('EV_SWEEP', ','.join('%sf' % float(ev) for ev in EVS))
    source = source.replace('EV_FUNCTION', function(env, 'F32 localLightEVScale('))
    source = source.replace('SELECTION_HELPER', helper).replace('SELECTION_LOOP', loop).replace('ADMISSION', admission)
    source = source.replace('CHANNEL_FUNCTION', function(pipe, 'LLVector4 LLPipeline::deferredLightShape(').replace('LLPipeline::', 'Pipeline::'))
    colour_start = pipe.index('    const F32 ls = light_scale * ALEnvIntensity::localLightEVScale(volume);', pipe.index('renderAreaShadows(debugLightingTerm()'))
    colour = pipe[colour_start:pipe.index('                    if (s <= 0.001f)', colour_start)]
    source = source.replace('DEFERRED_COLOUR', colour.replace('continue;', 'return false;'))
    probe = tree.read('indra/newview/llreflectionmapmanager.cpp')
    probe_start = probe.index('    const F32 ls = light_scale * ALEnvIntensity::localLightEVScale(light);')
    source = source.replace('PROBE_COLOUR', probe[probe_start:probe.index('\n\n', probe_start)])
    gate_start = probe.index('    const LLColor3 selection_color', probe_start) if '    const LLColor3 selection_color' in probe[probe_start:] else probe.index('    if (s.mRadius * 1.5f <=', probe_start)
    source = source.replace('PROBE_ELIGIBILITY', probe[gate_start:probe.index('    s.mEligible = eligible;', gate_start)])
    for header in ('alareashadowmath.h', 'alworldshadowmath.h'):
        (out / header).write_text(tree.read('indra/newview/' + header), encoding='utf-8')
    cpp, exe = out / 'selection.cpp', out / 'selection.exe'
    cpp.write_text(source, encoding='utf-8')
    cmd = out / 'compile.cmd'
    vcvars = r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc "%s" /Fe:"%s" /Fo:"%s"\n' % (vcvars, cpp, exe, out / 'selection.obj'))
    build = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=out, capture_output=True, text=True)
    assert build.returncode == 0, build.stdout + build.stderr
    return json.loads(subprocess.check_output([str(exe)], text=True))


def key(row):
    return tuple(row[n] for n in ('path', 'rig', 'include', 'competition', 'thin', 'level'))


def violations(rows):
    zero = {key(row): row for row in rows if row['ev'] == 0}
    failures = []
    for row in rows:
        base = zero[key(row)]
        for field in ('ids', 'scores', 'slot', 'strength', 'regular', 'drawn', 'native', 'plan', 'world_rank', 'picked_rank', 'probe_eligible'):
            if row[field] != base[field]:
                failures.append(dict(case=key(row), ev=row['ev'], field=field, before=base[field], after=row[field]))
        factor = 2 ** row['ev'] if not row['rig'] or row['include'] else 1
        if row['drawn'] and base['drawn']:
            assert np.isclose(row['radiance'], base['radiance'] * factor, rtol=2e-7), 'radiance or rig exclusion changed'
        assert np.isclose(row['probe_radiance'], base['probe_radiance'] * factor, rtol=2e-7), 'probe signature must track exposed radiance'
    return failures


def compose_program(r, path):
    """The actual point/projector visibility lookup and composition statement."""
    name = 'spotLightF' if path % 2 else 'pointLightF'
    text = r.tree.read(G.SHADERS + '/class3/deferred/' + name + '.glsl')
    visibility = text[text.index('uniform int relight_enabled;'):text.index('out vec4 frag_color;')]
    if path % 2:
        body = text[text.index('    float shadow = 1.0;'):text.index('    GBufferInfo gb =')]
        body += '\n    frag_color=vec4(vec3(radiance*shadow),shadow);'
    else:
        start = text.index('    if ((relight_enabled & 2) != 0 && light_shape.w >= 0.0)', text.index('void main()'))
        body = '    vec3 final_color=vec3(radiance);\n' + text[start:text.index('    float final_scale', start)]
        body += '\n    frag_color=vec4(final_color,areaShadowVisibility(tc,light_shape.w));'
    fragment = visibility + '''
out vec4 frag_color;
uniform vec4 light_shape;
uniform vec2 screen_res;
uniform float radiance;
uniform int proj_shadow_idx;
uniform float shadow_fade;
// Native PCF is a controlled input; its combination with the soft channel is
// production code. All cube depths/gather/filter/resolve remain real GL.
float sampleSpotShadow(vec3 pos,vec3 norm,int index,vec2 uv){return .8;}
vec4 getNorm(vec2 uv){return vec4(0,0,1,0);}
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 pos=vec3(0,0,-8);
''' + body + '\n}'
    gl = r.gl
    program = gl.program()
    gl.attach(program, r._compile('in vec3 position;void main(){gl_Position=vec4(position,1);}', 'EV composition vertex', 0x8B31))
    gl.attach(program, r._compile(fragment, name + ' EV composition'))
    gl.link(program)
    ok = gl.I(); gl.getprog(program, 0x8B82, C.byref(ok))
    if not ok.value:
        log = C.create_string_buffer(32000); gl.plog(program, len(log), None, log)
        raise AssertionError(log.value.decode())
    return program


def gl_checks(rows, before, ref, out, glm_include):
    gl = G.GL()
    assert not any(s in gl.renderer.lower() for s in ('gdi generic', 'llvmpipe', 'softpipe', 'swiftshader', 'lavapipe', 'software', 'basic render')), 'hardware GPU required'
    host = W.build_host(out, glm_include)
    try:
        # Keep EV's historical selection/lighting oracle and the same world
        # receiver algorithm on both sides of its exact EV-zero comparison.
        current_tree = G.SourceTree(ref)
        runners = [W.RealRunner(W.PreLookTree(current_tree, BASE), host, gl=gl),
                   W.RealRunner(current_tree, host, gl=gl)]
        programs = {id(r): [compose_program(r, path) for path in range(4)] for r in runners}
        # This receiver spans the native fade-to-screen hand-over too. Native
        # visibility above is deliberately independent of EV and the cube path
        # disables it only when the compiled production selector transfers it.
        for r in runners:
            for program in (r.gather, r.filter):
                gl.use(program); gl.uf[2](gl.loc(program, b'area_native_clip'), 2, 6)
        scene, light = W.fixture(emitter='none')
        cam = S.Camera((3.1, -4.4, 2.9), (0, 0, .4), 320, 180)
        g = S.cast_primary(scene, cam)
        _, depth, norm = S.encode_gbuffer(cam, g)
        near = CS.emitter_near(scene, light)
        for r in runners:
            r.gpu_atlas = W.Raster(r).atlas(scene, light, near)
        floor = (g['surf'] == 0) & (np.linalg.norm(g['pos'][..., :2], axis=2) < 8)
        cache = {}

        def render(row, r):
            path, native = row['path'], row['native']
            signature = (id(r), path, native, tuple(row['plan']), row['strength'], row['regular'])
            if signature not in cache:
                samples, steps, half, near_steps, rest_steps = row['plan']
                gl.use(r.gather)
                gl.uf[4](gl.loc(r.gather, b'area_dropout'), 1-row['strength'], 0, 0, 0)
                gl.uf[4](gl.loc(r.gather, b'area_regular_mix'), row['regular'], 0, 0, 0)
                gl.ui(gl.loc(r.gather, b'area_rest_steps'), rest_steps)
                world = dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), channels=[
                    dict(row=0, near=near, far=light.range, r_map=light.radius, seed=light.seed,
                         contact=.15 if native or path >= 2 else 0, contact_fade=float(native), owned=path >= 2)])
                visibility = r.run(cam, depth, norm, [light], dict(samples=samples, steps=steps, half=half, near_steps=near_steps), world=world)[0]
                cache[signature] = visibility
            visibility = cache[signature]
            rgba = np.ones((cam.H, cam.W, 4), np.float32)
            if row['slot'] >= 0:
                rgba[:, :, int(row['slot'])] = visibility
            # Floating output avoids display clipping concealing a shadow.
            target = r._texture('ev_output', 0, 0x8814, cam.W, cam.H, 0x1908, 0x1406, None)
            gl.bind_fbo(0x8D40, r.fbo); gl.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, target, 0)
            assert gl.fbo_status(0x8D40) == 0x8CD5
            gl.viewport(0, 0, cam.W, cam.H)
            r._texture('ev_mask', 7, 0x8058, cam.W, cam.H, 0x1908, 0x1406, rgba)
            pr = programs[id(r)][path]; r._begin(pr)
            gl.ui(gl.loc(pr, b'relight_enabled'), 2)
            for name in (b'area_shadow_map0', b'area_shadow_map1'): gl.ui(gl.loc(pr, name), 7)
            gl.uf[4](gl.loc(pr, b'light_shape'), light.radius, 0, 1, row['slot'])
            gl.uf[2](gl.loc(pr, b'screen_res'), cam.W, cam.H)
            gl.uf[1](gl.loc(pr, b'radiance'), row['radiance'])
            gl.ui(gl.loc(pr, b'proj_shadow_idx'), 0 if native else -1)
            gl.uf[1](gl.loc(pr, b'shadow_fade'), 0)
            gl.draw(4, 0, 3)
            pixels = np.empty((cam.H, cam.W, 4), np.float32)
            gl.read(0, 0, cam.W, cam.H, 0x1908, 0x1406, pixels.ctypes.data)
            assert np.isfinite(pixels).all(), 'nonfinite GL output'
            return pixels

        failures, original_failures, compared = [], [], 0
        zero, base_zero = {}, {}
        # Bright fixtures cover every rig/inclusion/path/quality/competition
        # combination; faint eligibility boundaries are exercised by C++ above.
        for source, sink, refs, r in ((before, original_failures, base_zero, runners[0]), (rows, failures, zero, runners[1])):
            for row in source:
                if row['level'] != 0: continue
                pixels = render(row, r)
                k = key(row)
                if row['ev'] == 0:
                    refs[k] = pixels
                    if refs is zero:
                        assert pixels.tobytes() == base_zero[k].tobytes(), 'EV 0 GL byte identity'
                    if row['slot'] >= 0:
                        assert np.any(pixels[:, :, 3][floor] < .7), ('positive shadow control', k)
                        assert np.any((pixels[:, :, 3][floor] > .1) & (pixels[:, :, 3][floor] < .9)), ('penumbra control', k)
                else:
                    reference = refs[k]
                    same_shape = pixels[:, :, 3].tobytes() == reference[:, :, 3].tobytes()
                    factor = 2 ** row['ev'] if not row['rig'] or row['include'] else 1
                    same_brightness = np.allclose(pixels[:, :, :3], reference[:, :, :3] * factor, rtol=4e-7, atol=1e-8)
                    if not same_shape or not same_brightness:
                        sink.append(dict(case=k, ev=row['ev'], shape=same_shape, brightness=same_brightness))
                    compared += 1
        assert any(.399 < r['ev'] < .401 and not r['shape'] for r in original_failures), 'EV .4 GPU negative control survived'
        print('[local-ev GL] GPU:', gl.renderer, flush=True)
        print('[local-ev GL] %d comparisons; %d failures; original %d failures; EV 0 byte-identical' % (compared, len(failures), len(original_failures)), flush=True)
        return dict(renderer=gl.renderer, failures=failures, original_failures=original_failures, comparisons=compared)
    finally:
        host.terminate(); host.wait()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-ref', default='WORKTREE')
    ap.add_argument('--glm-include', help='installed GLM headers, as for relight_world_gl_check')
    ap.add_argument('--out', default='.tmp/localev/check')
    args = ap.parse_args()
    out = Path(args.out).resolve()
    before = compile_selection(BASE, out / 'before')
    rows = compile_selection(args.source_ref, out / 'current')
    failures = violations(rows)
    controls = violations(before)
    assert any(x['ev'] > .399 and x['ev'] < .401 and x['field'] == 'slot' for x in controls), 'original EV .4 defect was not reproduced'
    probe = [r for r in before if key(r) == (0, 1, 0, 1, 0, 0)]
    assert all(r['slot'] >= 0 for r in probe if 0 <= r['ev'] < .399), 'repro must retain its channel through EV .39'
    assert all(r['slot'] < 0 for r in probe if .399 < r['ev'] < .401), 'repro must lose its channel at EV .4'
    before_zero = {key(r): r for r in before if r['ev'] == 0}
    assert all(r == before_zero[key(r)] for r in rows if r['ev'] == 0), 'EV 0 host bytes/selection drifted'
    gpu = gl_checks(rows, before, args.source_ref, out, args.glm_include)
    result = dict(source=args.source_ref, rows=rows, failures=failures, original_failures=controls, gpu=gpu, passed=not failures and not gpu['failures'])
    (out / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('[local-ev] %s: %d C++ cases, %d invariant failures; original defect: %d failures' % ('FAIL' if failures else 'PASS', len(rows), len(failures), len(controls)), flush=True)
    print('[local-ev] original mixed-light channel at EV 0/.2/.39/.4/1/2/-1:', [r['slot'] for r in probe], flush=True)
    if failures:
        print('[local-ev] first failures:', failures[:5], flush=True)
    return not result['passed']


if __name__ == '__main__':
    raise SystemExit(main())
