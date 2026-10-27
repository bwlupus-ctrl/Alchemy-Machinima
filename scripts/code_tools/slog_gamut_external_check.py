"""Round 3 external Sony CODE anchors; production GLSL, no viewer build.

Only A6 uses the production decoder, explicitly as a consistency check.
Native probes compile the unchanged production upload helper, the complete
Prism upload method, and the colorCorrect upload prefix against a GL seam.
This is not evidence that a running viewer dispatched a particular route.
Run: PYTHONUTF8=1 python scripts/code_tools/slog_gamut_external_check.py --gpu
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import numpy as np
import slog_gamut_check as S
# Baseline = the commit this feature was built on (pre Log gamut); not HEAD, which
# includes the feature once committed/merged. Override with env var.
import os as _os_base
BASE_REF = _os_base.environ.get('SLOG_GAMUT_EXTERNAL_CHECK_BASE', 'b8a911596c5')

OUT = S.ROOT/'.tmp/slog_gamut/tests3'
RESULT = {}


def sony(x):
    """Sony Technical Summary v1.0 p.7; independent published OETF."""
    x = np.maximum(np.asarray(x, dtype=float), 0.)
    return np.clip(np.where(x >= .01125,
        (420.+261.5*np.log10((x+.01)/.19))/1023.,
        (95.+x*(171.2102946929-95.)/.01125)/1023.), 0., 1.)


def xyz(primaries, white_xy=(.3127,.3290)):
    # Independently typed Sony p.8 chromaticities and D65, not shader literals.
    basis = np.array([[x/y, 1., (1.-x-y)/y] for x, y in primaries]).T
    wx,wy=white_xy
    white = np.array([wx/wy, 1., (1.-wx-wy)/wy])
    return basis @ np.diag(np.linalg.solve(basis, white))


XYZ709 = xyz(((.640, .330), (.300, .600), (.150, .060)))
XYZSG3 = xyz(((.730, .280), (.140, .855), (.100, -.050)))
SG3 = np.linalg.solve(XYZSG3, XYZ709)


def close(actual, expected, tol=.001):
    np.testing.assert_allclose(actual, expected, atol=tol, rtol=0.)


def record(anchor, status='PASS', **evidence):
    RESULT[anchor] = dict(status=status, **evidence)
    printed={k:v for k,v in evidence.items() if k!='programs'}
    print(status+' '+anchor+': '+json.dumps(printed), flush=True)


def killed(actual, expected, tol):
    # Compile failures never count as a killed mutation.
    assert np.max(np.abs(np.asarray(actual)-expected)) > tol, 'mutation survived'


def local_aces_search():
    roots = [Path(base)/name for base in ('C:/Program Files', 'C:/Program Files (x86)', 'C:/ProgramData')
             for name in ('VEGAS', 'VEGAS Pro', 'Red Giant', 'Maxon', 'Common Files',
                          'BorisFX', 'MAGIX', 'Blackmagic Design', 'Adobe')]
    roots += [Path('I:/Vegas')]
    configs, exact, modules, errors = [], [], [], []
    for root in roots:
        if not root.is_dir(): continue
        def onerror(error): errors.append(str(error))
        for directory, dirs, files in os.walk(root, onerror=onerror, followlinks=False):
            dirs[:] = [d for d in dirs if not any(t in d.lower() for t in
                       ('ref_private', 'reeelight', 'reelight', 'reshade', 'resahde', 'immerse'))]
            for name in files:
                path = Path(directory)/name
                if 'pyopencolorio' in name.lower(): modules.append(str(path))
                if path.suffix.lower() != '.ocio': continue
                configs.append(str(path))
                text = path.read_text(encoding='utf-8', errors='replace')
                if 'Input - Sony - S-Log3 - S-Gamut3' in text and any(t in str(path).lower() for t in ('aces_1.2','aces 1.2')):
                    exact.append(str(path))
    result = dict(roots=[str(p) for p in roots if p.is_dir()], configs=configs,
                  exact_configs=exact, local_python_modules=modules, errors=errors,
                  PyOpenColorIO_available=importlib.util.find_spec('PyOpenColorIO') is not None)
    (OUT/'aces_search.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    return result


def native_upload(r):
    """Real C++ upload arithmetic/settings lookup, GL API boundary instrumented."""
    pipe = (S.ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    from sensor_post_dispatch_check import body
    helper = pipe[pipe.index('static void uploadSLogCaptureUniforms('):]
    helper = helper[:helper.index('\n}')+2]
    prism = body(pipe, 'uploadPrismPreviewColorCorrectUniforms')
    method = body(pipe, 'colorCorrect')
    start = method.index('        if (apply_tonemap)')
    stop = method.index('            // [TonemapLegacySky]', start)
    prefix = method[start:stop]+'        }\n'
    # Keep the production route, predicate and helper invocation verbatim.
    extracted = helper+'\n'+prism+'\nvoid uploadColorCorrectPrefix(LLGLSLShader* shader, bool apply_tonemap) {\n'+prefix+'}\n'
    keys = sorted(set(re.findall(r'LLShaderMgr::([A-Z0-9_]+)', extracted)))
    enums = ','.join(keys)
    names = ','.join('"'+k.lower()+'"' for k in keys)
    pre = r'''
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
using S32=int; using F32=float; using U32=unsigned;
template<class T>T llclamp(T x,T a,T b){return std::clamp(x,a,b);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
static std::map<std::string,double> settings;
struct Settings{} gSavedSettings;
template<class T>struct LLCachedControl{
 std::string key; LLCachedControl(Settings&,const char* k,T v):key(k){if(!settings.count(k))settings[k]=double(v);}
 T operator()()const{return T(settings[key]);} operator T()const{return operator()();}
};
using Callback=void(*)(const char*,int,int,float,float,float,float);
static Callback callback;
struct LLGLSLShader{
 void uniform1i(int k,int x){callback(uniformNames[k],1,x,0,0,0,0);}
 void uniform1f(int k,float x){callback(uniformNames[k],2,0,x,0,0,0);}
 void uniform2f(int k,float x,float y){callback(uniformNames[k],3,0,x,y,0,0);}
 void uniform4f(int k,float x,float y,float z,float w){callback(uniformNames[k],4,0,x,y,z,w);}
 void uniform4uiv(int,int,const U32*){}
};
struct LLSettingsSky{using ptr_t=LLSettingsSky*;float getGamma(){return 1.f;}float getTonemapMix(bool){return 1.f;}};
struct LLEnvironment{static LLEnvironment& instance(){static LLEnvironment e;return e;}LLSettingsSky* getCurrentSky(){return nullptr;}};
namespace ALLPM{enum{CONTROL_BLOCK_WORDS=96};template<class...T>void setup709(T...){} }
struct LLPipeline{
 void uploadPrismPreviewColorCorrectUniforms(LLGLSLShader&) const;
 void colorCorrectWillApplyExposure(bool,bool* legacy,bool* no_post)const{*legacy=false;*no_post=false;}
};
'''
    pre = pre.replace('using Callback=', 'namespace LLShaderMgr{enum{'+enums+'};}\nstatic const char* uniformNames[]={'+names+'};\nusing Callback=')
    post = r'''
extern "C" __declspec(dllexport) void upload(Callback cb,int prism,int gamut,float purity){
 callback=cb;settings["AlchemyRenderTonemapType"]=8;settings["AlchemyToneMapSLogGamut"]=gamut;
 settings["AlchemyToneMapSLogPurity"]=purity;LLGLSLShader shader;
 if(prism)LLPipeline{}.uploadPrismPreviewColorCorrectUniforms(shader);
 else uploadColorCorrectPrefix(&shader,true);
}
'''
    vc = sorted((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    callbacks, dlls = [], []
    for variant in ('production', 'missing_upload'):
        source = extracted if variant == 'production' else extracted.replace('uploadSLogCaptureUniforms(shader);', ';').replace('uploadSLogCaptureUniforms(*shader);', ';')
        path = OUT/(variant+'.cpp')
        path.write_text(pre+source+post, encoding='utf-8')
        dll = OUT/(variant+'.dll')
        cmd = OUT/(variant+'.cmd')
        cmd.write_text('@echo off\ncall "'+str(vc)+'" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /O2 /LD "'+str(path)+'" /Fe:"'+str(dll)+'" /Fo"'+OUT.as_posix()+'/" /link /IMPLIB:"'+str(OUT/(variant+'.lib'))+'"\n', encoding='utf-8')
        run = subprocess.run(['cmd','/c',str(cmd)], cwd=OUT, capture_output=True, text=True, timeout=90)
        (OUT/(variant+'_compile.log')).write_text(run.stdout+run.stderr, encoding='utf-8')
        assert run.returncode == 0, run.stdout+run.stderr
        lib = C.CDLL(str(dll))
        cbtype = C.CFUNCTYPE(None,C.c_char_p,C.c_int,C.c_int,*([C.c_float]*4))
        lib.upload.argtypes=[cbtype,C.c_int,C.c_int,C.c_float]
        def forward(name, kind, integer, x,y,z,w):
            values = integer if kind==1 else x if kind==2 else (x,y) if kind==3 else (x,y,z,w)
            r.uniform(active[0], name.decode(), values)
        callbacks.append(cbtype(forward)); dlls.append(lib)
    active = [0]
    def upload(program, prism=False, missing=False, gamut=3, purity=1.):
        active[0]=program
        r.gl.use(program)
        dlls[int(missing)].upload(callbacks[int(missing)], int(prism), gamut, purity)
    return upload


def gpu():
    import sensor_post_gl_check as P
    r = P.Runner()
    production_hash = hashlib.sha256((S.SHADERS/S.TONE).read_bytes()).hexdigest()
    production_hashes={p:hashlib.sha256((S.SHADERS/p).read_bytes()).hexdigest() for p in
                       (S.TONE,'class1/alchemy/colorCorrectF.glsl')}
    common = P.common_modules()+(S.TONE,'class1/deferred/LPMUtil.glsl')
    def link(fragment, defs='', tone=None, modules=(), vertex=None):
        program = r.gl.program()
        for module in common+modules:
            r.gl.attach(program, r.compile(tone if module==S.TONE and tone is not None else r.source(module), defs))
        r.gl.attach(program,r.compile(fragment, defs))
        r.gl.attach(program,r.base.compile(vertex,0x8B31) if vertex else r.base.vertex)
        r.gl.link(program)
        ok=C.c_int(); r.gl.getprog(program,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000); r.gl.plog(program,len(log),None,log)
            raise AssertionError(log.value.decode())
        return program
    probe = '''in vec2 vary_fragcoord; uniform sampler2D diffuseRect; out vec4 frag_color;
vec3 encode_slog_family(vec3 c); vec3 linear_to_srgb(vec3 c);
void main(){frag_color=vec4(linear_to_srgb(encode_slog_family(texture(diffuseRect,vary_fragcoord).rgb)),1);}
'''
    tone = r.source(S.TONE)
    pr = link(probe)
    codetone=tone.replace('return srgb_to_linear(code);','return code;')
    assert codetone!=tone
    tap=probe.replace('linear_to_srgb(encode_slog_family(texture(diffuseRect,vary_fragcoord).rgb))','encode_slog_family(texture(diffuseRect,vary_fragcoord).rgb)')
    codeprogram=link(tap,tone=codetone)
    base = dict(tonemap_type=8,tonemap_slog_gamut=1,tonemap_slog_purity=1.,tonemap_params=(0.,0.,0.,0.),
                tonemap_grade=(1.,1.),tonemap_mix=1.,exposure=1.,gamma=1.,bloom_strength=0.)
    def draw(program, patches, **overrides):
        data=np.asarray(patches,np.float32).reshape(1,-1,3); width=data.shape[1]
        tex=r.upload('external_input',data,fmt=0x8814)
        return r.draw(program,r.target('external_output',width,1,fmt=0x8814),width,1,
                      [('diffuseRect',tex)],dict(base,**overrides))[0][0,:,:3].astype(float)
    red=draw(pr,[[1,0,0]])[0]
    close(red,[.5337,.3238,.2140])
    transposed=link(probe,tone=tone.replace('slf_gamut_matrix() * c','transpose(slf_gamut_matrix()) * c'))
    wrong=draw(transposed,[[1,0,0]])[0]; killed(wrong,[.5337,.3238,.2140],.001)
    close(wrong[1],.4793)
    record('A1',codes=red.tolist(),negative_transpose_codes=wrong.tolist(),negative_control='FAIL as required; memory only')
    neutral_results=[]
    for video,expected in ((0,[.5960,.5845,.4106]),(1,[.5729,.5630,.4141])):
        first=None
        for gamut in range(4):
            for purity in (*np.linspace(.6,1.,41),.73123):
                got=draw(pr,[[v]*3 for v in (1.,.9,.18)],tonemap_slog_gamut=gamut,
                         tonemap_slog_purity=purity,tonemap_params=(0.,float(video),0.,0.))
                close(got,np.broadcast_to(np.array(expected)[:,None],(3,3)))
                if first is None:first=got
                assert np.array_equal(got,first), 'neutral code identity'
        neutral_results.append(first[:,0].tolist())
    record('A2',full_legal=neutral_results,neutral_identity='4 gamuts x all 41 UI purity steps plus off-step 0.73123; equality branch covers entire interval')
    purityred=draw(pr,[[1,0,0]],tonemap_slog_purity=.9)[0]
    linear=S.slog3_inverse(purityred)  # published CPU formula, not our decoder
    close(linear,[.53070,.09053,.04138],1e-4)
    old='    c = slf_apply_purity(c);'
    assert old in tone
    swapped=tone.replace(old,'    // mutation: purity moved after gamut')
    # Insert immediately after the actual matrix block (before neutral toe).
    matrixline='        c = max(slf_gamut_matrix() * c, vec3(0.0));'
    assert matrixline in swapped
    swapped=swapped.replace(matrixline,matrixline+'\n    c = slf_apply_purity(c);')
    wrong=draw(link(probe,tone=swapped),[[1,0,0]],tonemap_slog_purity=.9)[0]
    killed(wrong,sony([.53070,.09053,.04138]),1e-4)
    close(S.slog3_inverse(wrong),[.52714,.08697,.03782],1e-4)
    record('A3',codes=purityred.tolist(),published_inverse_linear=linear.tolist(),negative_order='FAIL as required; memory only')

    upload=native_upload(r)
    getiv=r.gl.fn('glGetUniformiv',None,C.c_uint,C.c_int,C.POINTER(C.c_int))
    programs=[]
    color=r.source('class1/alchemy/colorCorrectF.glsl')
    # Runtime probe is an extra selectable branch; production main remains linked.
    # Godray main normally returns reconstructed scene light, so its encoder tap
    # must be selected explicitly rather than mislabelling that output as codes.
    instrumented=color.replace('void main()','void productionMain()',1)+'''
uniform int externalEncoderProbe;
void main(){if(externalEncoderProbe!=0)frag_color=vec4(cameraTransfer(texture(diffuseRect,vary_fragcoord).rgb),1);else productionMain();}
'''
    for stage in ('Legacy','Camera','Godrays'):
        for grade in (False,True):
            for legacy in (False,True):
                defs='#define TONEMAP 1\n#define HAS_POST_EFFECTS 1\n#define BLOOM_COMPOSITE 1\n'
                if stage!='Legacy':defs+='#define AL_SENSOR_HDR 1\n#define AL_SENSOR_'+stage.upper()+' 1\n'
                if grade:defs+='#define COLOR_GRADE 1\n'
                if legacy:defs+='#define LEGACY_GAMMA 1\n'
                modules=('class1/alchemy/postEffectUtilsF.glsl',)+(('class1/alchemy/colorGradeUtilF.glsl',) if grade else ())
                index=(2 if stage=='Camera' else 6)+int(grade)*2+int(legacy)
                name=(stage+f'[{index}]' if stage!='Legacy' else stage)+(' grade' if grade else '')+(' legacygamma' if legacy else '')
                programs.append((name,link(instrumented,defs,modules=modules),False,True))
    stub='''in vec2 vary_fragcoord; uniform sampler2D diffuseRect;
vec4 prismSignalSource(vec2 uv,vec3 p,vec3 n,bool incoming){return texture(diffuseRect,uv);}
bool prismSignalSourceInFrame(vec2 uv,bool incoming){return true;}
'''
    vertex='''#version 420
in vec2 position;out vec2 prism_uv;out vec2 vary_fragcoord;out vec3 prism_eye_pos;out vec3 prism_eye_normal;
void main(){gl_Position=vec4(position,0,1);prism_uv=position*.5+.5;vary_fragcoord=prism_uv;prism_eye_pos=vec3(0);prism_eye_normal=vec3(0,0,1);}
'''
    for path in ('prismPreviewF.glsl','prismProjectorFeedF.glsl'):
        for grade in (False,True):
            for legacy in (False,True):
                for look in ((False,True) if 'Feed' in path else (False,)):
                    defs='#define TONEMAP 1\n'+('#define COLOR_GRADE 1\n' if grade else '')+('#define LEGACY_GAMMA 1\n' if legacy else '')+('#define PROJECTOR_LOOK 1\n' if look else '')
                    modules=('class1/alchemy/colorGradeUtilF.glsl',) if grade else ()
                    name=path+f' grade={grade} legacygamma={legacy} projectorLook={look}'
                    programs.append((name,link(stub+r.source('class1/deferred/'+path),defs,modules=modules,vertex=vertex),True,False))
    # This source is currently dormant (no shader-manager recipe references it),
    # but fail closed by exercising it as an additional encoder consumer.
    programs.append(('dormant postDeferredTonemap',link(r.source('class1/deferred/postDeferredTonemap.glsl'),'#define GAMMA_CORRECT 1\n'),False,False))
    evidence=[]
    for name,program,prism,probe_branch in programs:
        location=r.gl.loc(program,b'tonemap_slog_gamut'); assert location>=0,name
        uniforms=dict(base,externalEncoderProbe=int(probe_branch),projectorFollowMainExposure=1,projectorLookLampGel=(1.,1.,1.))
        uniforms.pop('tonemap_slog_gamut');uniforms.pop('tonemap_slog_purity')
        data=r.upload('runtime_red',np.array([[[1,0,0]]],np.float32),fmt=0x8814)
        def run(missing=False):
            r.gl.use(program);r.gl.ui(location,0)
            upload(program,prism=prism,missing=missing)
            value=C.c_int();getiv(program,location,C.byref(value))
            got=r.draw(program,r.target('runtime',1,1,fmt=0x8814),1,1,[('diffuseRect',data)],uniforms)[0][0,0,:3]
            return value.value,got
        value,got=run();assert value==3,name
        close(got,sony([.63132,.03682,.01737]))
        missing,wrong=run(True);assert missing!=3,name+' missing upload survived readback'
        killed(wrong,sony([.63132,.03682,.01737]),.001)
        restored,again=run();assert restored==3;close(again,sony([.63132,.03682,.01737]))
        evidence.append(dict(program=name,glGetUniformiv=value,codes=got.tolist(),missing_upload='FAIL then restored'))
    record('A4',program_count=len(evidence),programs=evidence,
           scope='production C++ upload prefix/full Prism method + GL seam; viewer routing PENDING')

    # A5 50 only exceeds the ceiling when gamut=0,purity=1 (not SG3, where
    # its R becomes ~28). Explicitly state those controls instead of asserting
    # a false universal ceiling. The coloured-ray test uses SG3.
    hot=draw(codeprogram,[[50,.1,.1]],tonemap_slog_gamut=0)[0]
    assert hot[0]==1.;close(hot[1:],sony([.1,.1]))
    god=P.camera(r,'GODRAYS')
    def godray(scene,ray,gamut=1):
        st=r.upload('ceiling_scene',np.array([[scene]],np.float32),fmt=0x8814)
        rt=r.upload('ceiling_ray',np.array([[ray]],np.float32),fmt=0x8814)
        return r.draw(god,r.target('godray',1,1,fmt=0x8814),1,1,
                      [('diffuseRect',st),('diffuseMap',rt)],dict(base,tonemap_slog_gamut=gamut))[0][0,0,:3]
    ceiling=godray([50,.1,.1],[0,.0001,.0001],0)
    assert ceiling[0]==50.; assert np.all(ceiling[1:]>.1)
    blackcode=float(sony(0.))
    desired=np.array([blackcode,blackcode+.3,blackcode+.3])
    expected=np.maximum(np.linalg.solve(SG3,S.slog3_inverse(desired)),0.)
    coloured=godray([0,0,0],[0,.3,.3])
    assert coloured[0]==0.;assert np.all(coloured>=0.);close(coloured,expected,3e-5)
    record('A5',legacy_ceiling_codes=hot.tolist(),godray_ceiling=ceiling.tolist(),black_coloured_ray=coloured.tolist(),external_expected=expected.tolist())

    consistency='''in vec2 vary_fragcoord;uniform sampler2D diffuseRect;out vec4 frag_color;
vec3 encode_slog_family(vec3 c);vec3 decode_slog_family(vec3 c);vec3 linear_to_srgb(vec3 c);
void main(){frag_color=vec4(decode_slog_family(linear_to_srgb(encode_slog_family(texture(diffuseRect,vary_fragcoord).rgb))),1);}
'''
    cp=link(consistency); errors=[]
    for gamut in range(4):
        got=draw(cp,[[.6,.15,.12]],tonemap_slog_gamut=gamut,tonemap_slog_purity=.6,tonemap_params=(0.,1.,.3,4.))[0]
        close(got,[.6,.15,.12],4.888e-4);errors.append(float(np.max(np.abs(got-[.6,.15,.12]))))
        # Independent code reference also prevents a cancelling decoder error
        # from masking encoder order: primary-derived M, Sony OETF, legal map.
        linear=np.array([.6,.15,.12])*16.;y=linear@np.array([.2126,.7152,.0722]);linear=y+.6*(linear-y)
        matrix=np.linalg.solve(xyz(S.PRIMARIES[gamut]),XYZ709)
        expected=(64.+876.*sony(matrix@linear+.18*.3))/1023.
        close(draw(pr,[[.6,.15,.12]],tonemap_slog_gamut=gamut,tonemap_slog_purity=.6,
                   tonemap_params=(0.,1.,.3,4.))[0],expected,.001)
    record('A6',label='decoder consistency plus independent encoder codes',max_errors=errors,tolerance=4.888e-4)

    ramp=np.linspace(.090,.120,64); data=S.slog3_inverse(ramp)
    src=r.upload('shadow_ramp',np.repeat(data[None,:,None],3,axis=2),fmt=0x8814)
    output=[]
    quantisation=[]
    for ref in (None,BASE_REF):
        program=P.camera(r,'CAMERA',ref=ref)
        blit=r.program('class1/alchemy/blitWithEffectsF.glsl',modules=P.common_modules()+('class1/alchemy/postEffectUtilsF.glsl',),ref=ref)
        floatcodes=r.draw(program,r.target('float_ramp_'+str(ref),64,1,fmt=0x8814),64,1,[('diffuseRect',src)],
                          dict(base,tonemap_slog_gamut=0))[0][0,:,0]
        close(floatcodes,sony(np.maximum(data,0)),.001)
        for fmt in (0x8058,0x881A,0x8814): # RGBA8, RGBA16F, RGBA32F intermediates
            intermediate=r.target('chain_'+str(ref),64,1,fmt=fmt)
            r.draw(program,intermediate,64,1,[('diffuseRect',src)],dict(base,tonemap_slog_gamut=0))
            final=r.draw(blit,r.target('backbuffer_'+str(ref),64,1,fmt=0x8058),64,1,[('diffuseRect',intermediate[0])])[0][0,:,0]
            steps=np.diff(np.round(final*255).astype(int))
            assert np.all(steps>=0) and np.max(steps)<=1
            # A code near an 8-bit rounding boundary can differ by one output
            # bin because of inherited float OETF precision. The external
            # float-code anchor above tests correctness; this tests transport.
            quantisation.append(dict(ref=ref or 'worktree',intermediate=hex(fmt),max_step=int(np.max(steps)),
                                     external_code_error=float(np.max(np.abs(final-sony(np.maximum(data,0)))))))
            output.append(final)
    assert all(np.array_equal(a,b) for a,b in zip(output[:3],output[3:])),'HEAD quantisation changed'
    # Deliberately quantise precompensated LINEAR values, then display encode:
    # this must fail the same <=1 code step predicate.
    linear_fragment=probe.replace('linear_to_srgb(encode_slog_family(texture(diffuseRect,vary_fragcoord).rgb))','encode_slog_family(texture(diffuseRect,vary_fragcoord).rgb)')
    lp=link(linear_fragment)
    pre=r.draw(lp,r.target('wrong_linear8',64,1,fmt=0x8058),64,1,[('diffuseRect',src)],dict(base,tonemap_slog_gamut=0))[0][0,:,0]
    encoded=np.where(pre<=.0031308,12.92*pre,1.055*np.maximum(pre,0)**(1/2.4)-.055)
    badsteps=np.diff(np.round(encoded*255).astype(int));assert np.max(badsteps)>1
    record('A7',head_byte_identical=True,chains=quantisation,negative_linear_RGBA8_max_step=int(np.max(badsteps)),scope='production camera -> RGBA8/16F/32F -> production final blit -> RGBA8; OS/capture path PENDING')

    search=local_aces_search()
    ocio=None
    if search['exact_configs']:
        import slog_gamut_ocio_check as O
        patches=np.array([[1,0,0],[0,1,0],[0,0,1],[1,1,1],[.18,.18,.18],[.6,.15,.12],[.8,.35,.3]],float)
        codes=draw(pr,patches)
        ocio=O.run(OUT,Path(search['exact_configs'][0]),codes.tolist())
        assert ocio['status']=='PASS', 'Exact ACES 1.2 config found; OCIO runtime comparison PENDING: '+str(ocio)
        bradford=np.array([[.8951,.2664,-.1614],[-.7502,1.7135,.0367],[.0389,-.0685,1.0296]])
        d65=np.array([.3127/.3290,1.,.3583/.3290])
        aceswhite=np.array([.32168/.33767,1.,(1.-.32168-.33767)/.33767])
        adaptation=np.linalg.solve(bradford,np.diag((bradford@aceswhite)/(bradford@d65))@bradford)
        ap0=xyz(((.7347,.2653),(0.,1.),(.0001,-.077)),(.32168,.33767))
        expected=patches@np.linalg.solve(ap0,adaptation@XYZ709).T
        error=float(np.max(np.abs(np.array(ocio['AP0'])-expected)))
        # Gate on the adaptation the installed ACES 1.2 Sony IDT really uses
        # (CAT02; its matrix is verified against an independent CAT02 derivation
        # below). Bradford is kept as information only. Tolerance unchanged.
        cat02=np.array([[.7328,.4296,-.1624],[-.7036,1.6975,.0061],[.003,.0136,.9834]])
        adapt02=np.linalg.solve(cat02,np.diag((cat02@aceswhite)/(cat02@d65))@cat02)
        expected02=patches@np.linalg.solve(ap0,adapt02@XYZ709).T
        cat02error=float(np.max(np.abs(np.array(ocio['AP0'])-expected02)))
        close(ocio['AP0'],expected02,1e-3)
        configtext=Path(ocio['config']).read_text(encoding='utf-8')
        sonyspace=configtext.split('    name: Input - Sony - S-Log3 - S-Gamut3\n',1)[1].split('  - !<ColorSpace>',1)[0]
        literal=re.search(r'matrix: \[([^]]+)\]',sonyspace)
        assert literal,'Sony IDT matrix not found in exact local ACES 1.2 config'
        idtmatrix=np.array([float(v) for v in literal[1].split(',')]).reshape(4,4)[:3,:3]
        cat02matrix=np.linalg.solve(ap0,adapt02@XYZSG3)
        close(idtmatrix,cat02matrix,5e-6)
        ocio.update(status='PASS' if cat02error<=1e-3 else 'FAIL',max_AP0_error=error,tolerance=1e-3,
                    max_CAT02_AP0_error=cat02error,CAT02_AP0=expected02.tolist(),patches=patches.tolist(),
                    config_Sony_IDT_matrix=idtmatrix.tolist(),independent_CAT02_matrix=cat02matrix.tolist(),
                    max_CAT02_matrix_error=float(np.max(np.abs(idtmatrix-cat02matrix))),
                    reference='independent Rec709 -> XYZ D65 -> CAT02 ACES white -> AP0 (as the installed Sony IDT); Bradford error informational',
                    GPU_codes=codes.tolist(),expected_AP0=expected.tolist(),config_sha256=hashlib.sha256(Path(ocio['config']).read_bytes()).hexdigest())
        (OUT/'ocio_comparison.json').write_text(json.dumps(ocio,indent=2)+'\n',encoding='utf-8')
    primaries=draw(pr,np.eye(3))
    recovered=S.slog3_inverse(primaries)
    error=float(np.max(np.abs(recovered@XYZSG3.T-XYZ709.T)))
    # Delivered sRGB approximation can exceed 1e-5 in XYZ at bright channels.
    # Tap production codes before srgb_to_linear to isolate matrix correctness.
    exactcodes=draw(codeprogram,np.eye(3))
    exactlinear=S.slog3_inverse(exactcodes)
    xyzerror=float(np.max(np.abs(exactlinear@XYZSG3.T-XYZ709.T)))
    close(exactlinear@XYZSG3.T,XYZ709.T,1e-5)
    record('A8',status=ocio['status'] if ocio else 'PASS',ACES=({'config':ocio['config'],'PyOpenColorIO':ocio['version'],'max_Bradford_AP0_error':ocio['max_AP0_error'],'tolerance':1e-3,
           'max_CAT02_AP0_error':ocio['max_CAT02_AP0_error'],'explanation':'Gate uses CAT02 like the installed ACES 1.2 Sony IDT; Bradford error informational only'} if ocio else 'Exact ACES 1.2 config not found locally; independent Sony XYZ fallback'),configs_found=len(search['configs']),
           exact_config_count=len(search['exact_configs']),additional_XYZ_check='GPU code tap -> published Sony inverse -> independent SG3 XYZ equals Rec709 XYZ',
           max_XYZ_error=xyzerror,delivered_XYZ_error=error,tolerance=1e-5)
    assert hashlib.sha256((S.SHADERS/S.TONE).read_bytes()).hexdigest()==production_hash,'production source mutated'
    assert all(hashlib.sha256((S.SHADERS/p).read_bytes()).hexdigest()==digest for p,digest in production_hashes.items())
    RESULT['provenance']=dict(shader_sha256=production_hash,production_shader_hashes=production_hashes,renderer=r.gl.renderer,negative_mutations='in-memory/extracted test code only; production files unchanged')


def ui():
    from xml.etree import ElementTree as ET
    files=('floater_lightbox_settings.xml','panel_dpage_look_image.xml')
    for file in files:
        tree=ET.parse(S.ROOT/'indra/newview/skins/default/xui/en'/file)
        node=next(x for x in tree.iter() if x.get('control_name')=='AlchemyToneMapSLogGamut')
        # Kit row owns help for generated child combo.
        text=node.get('tool_tip','')
        if not text:
            text=next(x.get('help','') for x in tree.iter('kit_row') if x.get('setting')=='AlchemyToneMapSLogGamut')
        assert 'Cineon and S-Log2: use Rec.709 unless your tool expects otherwise.' in text
        assert 'S-Log3 with S-Gamut3 or S-Gamut3.Cine' in text
    record('A9',pairing_guidance=list(files),default='S-Gamut3; S-Log2/Cineon guidance explicitly recommends Rec.709')


def main():
    os.environ['PYTHONUTF8']='1'
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gpu',action='store_true');parser.add_argument('--gpu-worker',action='store_true',help=argparse.SUPPRESS)
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    if args.gpu and not args.gpu_worker:
        if not S.gpu_idle():
            print('PENDING-GPU: rerun python scripts/code_tools/slog_gamut_external_check.py --gpu');return 2
        return subprocess.run([sys.executable,__file__,'--gpu-worker'],timeout=300).returncode
    ui()
    if args.gpu_worker:
        if not S.gpu_idle():return 2
        try:gpu()
        finally:(OUT/'results.json').write_text(json.dumps(RESULT,indent=2)+'\n',encoding='utf-8')
    return 1 if any(row.get('status')=='FAIL' for row in RESULT.values()) else 0


if __name__=='__main__':sys.exit(main())
