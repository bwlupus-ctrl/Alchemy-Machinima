"""Windows offscreen GPU compile/readback gate for Phase 1+2 production GLSL.
No viewer build, launch, or external reference shader is used.
"""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import numpy as np
import sys
import subprocess
from functools import lru_cache
from relight_prim_size_check import check_prim_size

prim_source_radii = check_prim_size()

# Read committed production sources without checking out/stashing the worktree.
shader_ref=sys.argv[sys.argv.index('--shader-ref')+1] if '--shader-ref' in sys.argv else None
@lru_cache(maxsize=None)
def read_shader(path):
    if shader_ref:
        return subprocess.check_output(['git','show',shader_ref+':'+Path(path).as_posix()],text=True)
    return Path(path).read_text()

user=C.WinDLL('user32'); gdi=C.WinDLL('gdi32'); ogl=C.WinDLL('opengl32')
def api(dll, name, result, *args):
    f=getattr(dll,name); f.restype=result; f.argtypes=args; return f
create=api(user,'CreateWindowExW',W.HWND,W.DWORD,W.LPCWSTR,W.LPCWSTR,W.DWORD,C.c_int,C.c_int,C.c_int,C.c_int,W.HWND,W.HMENU,W.HINSTANCE,C.c_void_p)
wnd=create(0,'STATIC','Gate shader test',0,0,0,16,16,None,None,None,None)
assert wnd, 'hidden WGL window'
dc=api(user,'GetDC',W.HDC,W.HWND)(wnd)
class PFD(C.Structure):
    _fields_=[('nSize',W.WORD),('nVersion',W.WORD),('dwFlags',W.DWORD)]+[(n,C.c_ubyte) for n in ['iPixelType','cColorBits','cRedBits','cRedShift','cGreenBits','cGreenShift','cBlueBits','cBlueShift','cAlphaBits','cAlphaShift','cAccumBits','cAccumRedBits','cAccumGreenBits','cAccumBlueBits','cAccumAlphaBits','cDepthBits','cStencilBits','cAuxBuffers','iLayerType','bReserved']]+[(n,W.DWORD) for n in ['dwLayerMask','dwVisibleMask','dwDamageMask']]
pfd=PFD();pfd.nSize=C.sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=0x25;pfd.cColorBits=32;pfd.cAlphaBits=8
fmt=api(gdi,'ChoosePixelFormat',C.c_int,W.HDC,C.POINTER(PFD))(dc,C.byref(pfd));assert fmt
assert api(gdi,'SetPixelFormat',W.BOOL,W.HDC,C.c_int,C.POINTER(PFD))(dc,fmt,C.byref(pfd))
ctx=api(ogl,'wglCreateContext',C.c_void_p,W.HDC)(dc);assert ctx
assert api(ogl,'wglMakeCurrent',W.BOOL,W.HDC,C.c_void_p)(dc,ctx)
getproc=api(ogl,'wglGetProcAddress',C.c_void_p,C.c_char_p)
I=C.c_int; U=C.c_uint; F=C.c_float; P=C.c_void_p
def gl(name,result,*args):
    ptr=getproc(name.encode())
    return C.WINFUNCTYPE(result,*args)(ptr) if ptr and ptr not in (1,2,3,2**64-1) else api(ogl,name,result,*args)
getstr=gl('glGetString',C.c_char_p,U)
print('GL',getstr(0x1F02).decode(),getstr(0x1F01).decode(),flush=True)
shader=gl('glCreateShader',U,U); source=gl('glShaderSource',None,U,I,C.POINTER(C.c_char_p),C.POINTER(I));compile_=gl('glCompileShader',None,U)
getshader=gl('glGetShaderiv',None,U,U,C.POINTER(I));slog=gl('glGetShaderInfoLog',None,U,I,C.POINTER(I),C.c_char_p)
program=gl('glCreateProgram',U);attach=gl('glAttachShader',None,U,U);link=gl('glLinkProgram',None,U);getprog=gl('glGetProgramiv',None,U,U,C.POINTER(I));plog=gl('glGetProgramInfoLog',None,U,I,C.POINTER(I),C.c_char_p)
bindattr=gl('glBindAttribLocation',None,U,U,C.c_char_p);use_raw=gl('glUseProgram',None,U)
loc=gl('glGetUniformLocation',I,U,C.c_char_p); uf=[None]+[gl('glUniform%df'%n,None,I,*([F]*n)) for n in range(1,5)];ui=gl('glUniform1i',None,I,I)
def use(pr):
    use_raw(pr)
    # Mirror the shared production bind hook, including its default scale.
    uf[1](loc(pr,b'relight_highlight_scale'),1.0)
    uf[1](loc(pr,b'relight_legacy_exponent'),368.0)
matrix=gl('glUniformMatrix4fv',None,I,I,U,C.POINTER(F));matrix3=gl('glUniformMatrix3fv',None,I,I,U,C.POINTER(F))
genTex=gl('glGenTextures',None,I,C.POINTER(U));bindTex=gl('glBindTexture',None,U,U);teximage=gl('glTexImage2D',None,U,I,I,I,I,I,U,U,P);param=gl('glTexParameteri',None,U,U,I);active=gl('glActiveTexture',None,U)
genfbo=gl('glGenFramebuffers',None,I,C.POINTER(U));bindfbo=gl('glBindFramebuffer',None,U,U);attachment=gl('glFramebufferTexture2D',None,U,U,U,U,I);status=gl('glCheckFramebufferStatus',U,U)
viewport=gl('glViewport',None,I,I,I,I);disable=gl('glDisable',None,U);read=gl('glReadPixels',None,I,I,I,I,U,U,P)
genbuf=gl('glGenBuffers',None,I,C.POINTER(U));bindbuf=gl('glBindBuffer',None,U,U);bufferdata=gl('glBufferData',None,U,C.c_ssize_t,P,U);enableattr=gl('glEnableVertexAttribArray',None,U);attrib=gl('glVertexAttribPointer',None,U,I,U,U,I,P);draw=gl('glDrawArrays',None,U,I,I)

root=Path('indra/newview/app_settings/shaders')
preamble="""
#define MAX_JOINTS_PER_MESH_OBJECT 110
#define REF_SAMPLE_COUNT 32
#define GBUFFER_FLAG_HAS_PBR 0.67
#define GBUFFER_FLAG_HAS_HDRI 1.0
#define GBUFFER_FLAG_HAS_ATMOS 0.34
#define GBUFFER_FLAG_SKIP_ATMOS 0.0
#define GET_GBUFFER_FLAG(data, flag) (abs(data-flag)<0.1)
#define texture2D texture
#define textureCube texture
vec4 diffuseLookup(vec2 uv);
struct GBufferInfo { vec4 albedo; vec4 specular; vec3 normal; vec4 emissive; float gbufferFlag; float envIntensity; };
"""
def compile_part(text, defines, label, stage=0x8B30):
    sh=shader(stage)
    injection=preamble+defines+'\n'
    # Mirror LLShaderMgr::loadShaderFile: defines go AT the marker, or at the
    # start only for utility files without a marker. Ordering is significant.
    prepared=text.replace('/*[EXTRA_CODE_HERE]*/',injection,1) if '/*[EXTRA_CODE_HERE]*/' in text else injection+text
    raw=('#version 420\n'+prepared).encode()
    src=C.c_char_p(raw);source(sh,1,C.byref(src),None);compile_(sh);ok=I();getshader(sh,0x8B81,C.byref(ok))
    if not ok.value:
        out=C.create_string_buffer(32000);slog(sh,len(out),None,out)
        raise AssertionError(label+' '+out.value.decode())
    return sh
files=['class1/deferred/deferredUtil.glsl','class3/deferred/multiPointLightF.glsl',
       'class3/deferred/pointLightF.glsl','class3/deferred/spotLightF.glsl',
       'class2/deferred/alphaF.glsl','class3/deferred/materialF.glsl',
       'class2/deferred/pbralphaF.glsl','class2/deferred/sharedActorFxPbrF.glsl',
       'class1/deferred/screenSpaceRayUtil.glsl','class3/deferred/areaShadowF.glsl',
       'class3/deferred/areaShadowFilterF.glsl']

# Resolve shader classes exactly like loadShaderFile (highest available <= level).
def production_file(name, level=3):
    for cls in range(level, 0, -1):
        candidate=root/('class%d'%cls)/name
        if candidate.exists(): return candidate
    raise AssertionError(name)

# Modules from LLShaderMgr::attachShaderFeatures. No generated function stubs:
# unresolved functions and vertex/fragment interface mismatches must fail linking.
def link_production(file, defines, label, test_fragment=None):
    name=Path(file).name
    forward=name in ('alphaF.glsl','materialF.glsl','pbralphaF.glsl','sharedActorFxPbrF.glsl')
    # Global basic-shader defines accompany the per-program permutations.
    if 'HAS_SUN_SHADOW' in defines: defines+='#define SUN_SHADOW 1\n'
    if name=='spotLightF.glsl': defines+='#define SPOT_SHADOW 1\n'
    if forward: defines+='#define REFMAP_LEVEL 1\n'
    fragment=['deferred/globalF.glsl','environment/srgbF.glsl',
              'windlight/atmosphericsVarsF.glsl','windlight/gammaF.glsl',
              'windlight/atmosphericsFuncs.glsl','windlight/atmosphericsF.glsl',
              'deferred/deferredUtil.glsl','deferred/gbufferUtil.glsl']
    if forward:
        fragment+=['windlight/atmosphericsHelpersF.glsl','windlight/cineHazeF.glsl',
                   'environment/waterFogF.glsl','deferred/shadowUtil.glsl']
        fragment+=['deferred/reflectionProbeF.glsl','deferred/screenSpaceReflUtil.glsl']
    if 'HAS_EMISSIVE' in defines or 'HAS_ACTOR_FX' in defines:
        fragment+=['alchemy/avatarLightResponseF.glsl']
    if 'HAS_ACTOR_FX' in defines:
        fragment+=['alchemy/actorFxDissolveF.glsl','alchemy/actorFxF.glsl']
    if name=='spotLightF.glsl': fragment+=['deferred/shadowUtil.glsl']
    if name in ('areaShadowF.glsl','areaShadowFilterF.glsl'):
        fragment+=['deferred/screenSpaceRayUtil.glsl']
    if name=='spotLightF.glsl': vertex='deferred/multiPointLightV.glsl' if 'MULTI_SPOTLIGHT' in defines else 'deferred/pointLightV.glsl'
    else: vertex='deferred/'+name.replace('F.glsl','V.glsl')
    if name in ('areaShadowF.glsl','areaShadowFilterF.glsl'): vertex='deferred/areaShadowV.glsl'
    vertices=[vertex,'deferred/textureUtilV.glsl']
    if forward:
        vertices+=['environment/srgbF.glsl','windlight/atmosphericsVarsV.glsl',
                   'windlight/atmosphericsHelpersV.glsl','windlight/atmosphericsFuncs.glsl',
                   'windlight/atmosphericsV.glsl']
    if 'HAS_SKIN' in defines: vertices+=['avatar/objectSkinV.glsl']
    if 'IS_AVATAR_SKIN' in defines: vertices+=['avatar/avatarSkinV.glsl']
    vertices+=['objects/indexedTextureV.glsl' if 'USE_INDEXED_TEX' in defines else 'objects/nonindexedTextureV.glsl']
    if '#define main actorMaskedMain' in defines:
        fragment+=['deferred/actorMaskGateF.glsl']
        vertices+=['deferred/actorMaskGateV.glsl']
    pr=program()
    for module in fragment:
        path=production_file(module, 1 if module in ('deferred/deferredUtil.glsl','deferred/gbufferUtil.glsl','deferred/globalF.glsl','deferred/shadowUtil.glsl') else 3)
        attach(pr,compile_part(read_shader(path),defines,label+' '+module))
    text=test_fragment if test_fragment is not None else read_shader(root/file)
    if 'USE_INDEXED_TEX' in defines:
        # LLShaderMgr's generated indexed diffuseLookup; use eight channels.
        lookup=''.join('uniform sampler2D tex%d;\n'%i for i in range(8))
        lookup+='flat in int vary_texture_index;\nvec4 diffuseLookup(vec2 uv) {\n'
        lookup+='switch(vary_texture_index) {\n'
        lookup+=''.join('case %d: return texture(tex%d,uv);\n'%(i,i) for i in range(8))
        lookup+='} return vec4(1,0,1,1); }\n'
        text+='\n'+lookup
    attach(pr,compile_part(text,defines,label))
    if test_fragment is not None:
        attach(pr,compile_part('void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}',defines,label+' fullscreen',0x8B31))
    else:
        for module in vertices:
            attach(pr,compile_part(read_shader(production_file(module)),defines,label+' '+module,0x8B31))
    link(pr);ok=I();getprog(pr,0x8B82,C.byref(ok))
    if not ok.value:
        out=C.create_string_buffer(64000);plog(pr,len(out),None,out)
        raise AssertionError(label+' LINK '+out.value.decode())
    # Both settings use this exact linked executable, including stale shape
    # storage when switching off. No compile-time master define is supplied.
    master=loc(pr,b'relight_enabled')
    if master >= 0:
        use(pr)
        for value in (1,0,1,0): ui(master,value)
    return pr

if '--area-only' in sys.argv:
    exec(compile(Path(__file__).with_name('relight_area_check.py').read_text(),
                 'relight_area_check.py', 'exec'))
    sys.exit(0)

def production_variants(file):
    name=Path(file).name
    actor='#define HAS_ACTOR_FX 1\n'
    maps='#define HAS_NORMAL_MAP 1\n#define HAS_SPECULAR_MAP 1\n#define HAS_EMISSIVE_MAP 1\n#define USE_VERTEX_COLOR 1\n'
    if name=='alphaF.glsl':
        base=actor+'#define USE_INDEXED_TEX 1\n#define USE_VERTEX_COLOR 1\n#define HAS_ALPHA_MASK 1\n'
        return [base,base+'#define HAS_SUN_SHADOW 1\n',base+'#define HAS_SKIN 1\n#define HAS_SUN_SHADOW 1\n',
                base.replace(actor,'')+'#define IS_HUD 1\n',
                actor+'#define USE_DIFFUSE_TEX 1\n#define USE_VERTEX_COLOR 1\n#define IS_AVATAR_SKIN 1\n#define HAS_SUN_SHADOW 1\n']
    if name=='materialF.glsl':
        result=[]
        for mode in range(4):
            for normal,spec in [(0,0),(0,1),(1,0),(1,1)]:
                result.append(actor+'#define DIFFUSE_ALPHA_MODE %d\n'%mode+
                    ('#define HAS_ALPHA_MASK 1\n' if mode else '')+
                    ('#define HAS_NORMAL_MAP 1\n' if normal else '')+
                    ('#define HAS_SPECULAR_MAP 1\n' if spec else ''))
        return result+[result[7]+'#define HAS_SKIN 1\n#define HAS_SUN_SHADOW 1\n']
    if name=='pbralphaF.glsl':
        base=actor+maps+'#define DIFFUSE_ALPHA_MODE 1\n'
        return [base,base+'#define HAS_SUN_SHADOW 1\n',base+'#define HAS_SKIN 1\n#define HAS_SUN_SHADOW 1\n#define ALR_RIMSPLIT_PARITY 1\n']
    if name=='sharedActorFxPbrF.glsl':
        result=[]
        for mode,alpha in [(0,0),(1,2),(2,1)]:
            base=actor+maps+'#define SHARED_ACTOR_FX_REPLAY 1\n#define SHARED_ACTOR_FX_ALPHA_MODE %d\n#define DIFFUSE_ALPHA_MODE %d\n'%(mode,alpha)
            if mode==1: base+='#define HAS_ALPHA_MASK 1\n'
            result+=[base,base+'#define SHARED_ACTOR_FX_SLOT_FILTER 1\n#define GLTF_INDEXED_CHANNELS 8\n#define HAS_SKIN 1\n#define HAS_SUN_SHADOW 1\n#define ALR_RIMSPLIT_PARITY 1\n']
        return result
    return None

count=0
linked=0
# One unconditional production program; master values are runtime uniforms.
shape=''
for file in ([] if '--rig-only' in sys.argv else files):
    variants=['']
    if 'multiPoint' in file:variants=['#define LIGHT_COUNT %d\n'%n for n in [1,8,16]]+['#define LIGHT_COUNT 16\n#define RIG_RIM_AUX 1\n#define HAS_EMISSIVE 1\n']
    if 'spotLight' in file:variants=['','#define MULTI_SPOTLIGHT 1\n','#define MULTI_SPOTLIGHT 1\n#define RIG_RIM_AUX 1\n#define HAS_EMISSIVE 1\n']
    if file.endswith('alphaF.glsl'):variants=['#define USE_DIFFUSE_TEX 1\n','#define USE_DIFFUSE_TEX 1\n#define HAS_ACTOR_FX 1\n','#define USE_INDEXED_TEX 1\n#define USE_VERTEX_COLOR 1\n#define HAS_ACTOR_FX 1\n#define HAS_SUN_SHADOW 1\n']
    if 'materialF' in file:variants=['#define DIFFUSE_ALPHA_MODE %d\n'%n for n in range(4)]+['#define DIFFUSE_ALPHA_MODE 1\n#define HAS_ACTOR_FX 1\n#define HAS_NORMAL_MAP 1\n#define HAS_SPECULAR_MAP 1\n']
    if 'pbralphaF' in file:variants=['','#define HAS_ACTOR_FX 1\n','#define HAS_ACTOR_FX 1\n#define ALR_RIMSPLIT_PARITY 1\n']
    if file.endswith('pointLightF.glsl'):variants=['','#define RIG_RIM_AUX 1\n#define HAS_EMISSIVE 1\n']
    if 'sharedActorFx' in file:variants=['#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_ALPHA_MODE %d\n'%n for n in [0,1,2]]+['#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_ALPHA_MODE 2\n#define ALR_RIMSPLIT_PARITY 1\n','#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_ALPHA_MODE 2\n#define SHARED_ACTOR_FX_SLOT_FILTER 1\n#define GLTF_INDEXED_CHANNELS 8\n#define HAS_SUN_SHADOW 1\n']
    if 'multiPoint' in file or 'spotLight' in file:
        variants+=[variants[-1]+'#define main actorMaskedMain\n']
    for variant in variants:
        compile_part(read_shader(root/file),shape+variant,file+' runtime '+variant)
        count+=1
        if production_variants(file) is None and not file.endswith(("deferredUtil.glsl","screenSpaceRayUtil.glsl")):
            link_production(file,shape+variant,file+" runtime "+variant)
            linked+=1
    for variant in production_variants(file) or []:
        link_production(file,shape+variant,file+' runtime '+variant)
        linked+=1
    print('[RelightQ] shader PASS',file,'runtime master',flush=True)
print('[RelightQ] shader compilation PASS',count,'production GLSL units/permutations',flush=True)

print('[RelightQ] production program linkage PASS',linked,flush=True)

# Execute the actual production helper bodies in a tiny test-only float target.
# This target belongs to this standalone process; no viewer target/state is touched.
util=read_shader(root/'class1/deferred/deferredUtil.glsl')
def function(name):
    start=util.index(name)
    start=util.rfind('\n',0,start)+1
    a=util.index('{',start); end=a+1; depth=1
    while depth:
        if util[end]=='{':depth+=1
        if util[end]=='}':depth-=1
        end+=1
    return util[start:end]
math_source='const float M_PI=3.14159265;\nconst float MIN_PBR_ROUGHNESS=.045;\n'+function('float calcLegacyDistanceAttenuation(')
math_source+=util[util.index('struct PBRInfo'):util.index('// [RigRim] ---')]
# Runtime uniforms make this exercise the GPU implementation rather than fold constants.
wrapper="""
uniform vec3 testLight;
uniform vec4 testShape;
uniform int testMode;
uniform sampler2D testGloss;
out vec4 result;
void main()
{
    vec3 n=vec3(0,0,1), v=n;
    if (testMode==0)
        result=vec4(relightAtten(length(testLight),15.0,.5,testShape),
                    relightRatio(n,testLight,testShape.x),
                    relightRepresentative(n,v,testLight,testShape.x).xz);
    else if (testMode==2)
    {
        vec3 highlight;
        vec3 total=relightLegacy(testGloss,vec3(.6),vec4(1),n,v,testLight,15.0,.5,testShape,highlight);
        result=vec4(total,highlight.r);
    }
    else
        result=vec4(relightPbr(vec3(.6),vec3(.04),.2,0.0,n,v,testLight,15.0,.5,testShape,1.0),1.0);
}
"""
pr=program();attach(pr,compile_part(math_source+wrapper,shape,'production helper readback'))
vs=shader(0x8B31)
vertex=C.c_char_p(b'#version 420\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}')
source(vs,1,C.byref(vertex),None);compile_(vs);attach(pr,vs);link(pr);ok=I();getprog(pr,0x8B82,C.byref(ok))
if not ok.value:
    out=C.create_string_buffer(32000);plog(pr,len(out),None,out);raise AssertionError(out.value.decode())
use(pr)
gl('glUniform2f',None,I,F,F)(loc(pr,b'relight_limits'),.1,4.0)
tex=U();genTex(1,C.byref(tex));bindTex(0x0DE1,tex);param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2600)
teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,None)
fbo=U();genfbo(1,C.byref(fbo));bindfbo(0x8D40,fbo);attachment(0x8D40,0x8CE0,0x0DE1,tex,0);assert status(0x8D40)==0x8CD5
viewport(0,0,1,1)
for cap in [0x0B71,0x0BE2,0x0C11,0x8DB9,0x0B44,0x0BD0]:disable(cap)
def sample(light,shape_value,mode):
    uf[3](loc(pr,b'testLight'),*light);uf[4](loc(pr,b'testShape'),*shape_value);ui(loc(pr,b'testMode'),mode)
    draw(4,0,3);pixels=np.empty(4,np.float32);read(0,0,1,1,0x1908,0x1406,pixels.ctypes.data)
    assert np.all(np.isfinite(pixels)),(light,shape_value,mode,pixels)
    return pixels
calibration=sample((0,0,7.5),(.3,1,128/9,-1),0)
assert abs(calibration[0]-2/9)<1.e-6,calibration
bound=sample((0,0,0),(.3,1,128/9,-1),0)
assert abs(bound[0]-32/9)<1.e-6 and bound[1]==1,bound
# Changing either limit updates the same linked program, without recompilation.
uniform2=gl('glUniform2f',None,I,F,F)
uniform2(loc(pr,b'relight_limits'),1.0,4.0)
assert abs(sample((0,0,0),(.1,1,1,-1),0)[0]-1.0)<1.e-6
uniform2(loc(pr,b'relight_limits'),.1,.25)
assert abs(sample((0,0,0),(.1,1,1,-1),0)[0]-2/9)<1.e-6
uniform2(loc(pr,b'relight_limits'),.1,4.0)
print('[RelightQ] live floor/boost uniforms PASS same program, no reload',flush=True)
representative=sample((1,0,2),(.5,0,1,-1),0)
assert abs(representative[2]-.5/np.sqrt(4.25))<1.e-6,representative
wrap=sample((2,0,-.2),(.5,0,1,-1),0)
assert wrap[1]>0,wrap
# Nonzero LUT makes a behind-surface specular leak observable.
gloss=U();genTex(1,C.byref(gloss));active(0x84C1);bindTex(0x0DE1,gloss)
param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2600)
one=np.ones(4,np.float32);teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,one.ctypes.data)
ui(loc(pr,b'testGloss'),1);active(0x84C0)
for light in [(0,0,-2),(2,0,-1),(.1,0,-.3)]:
    for physical in [0,1]:
        for mode in [1,2]:
            dark=sample(light,(.3,physical,128/9 if physical else 1,-1),mode)
            assert np.array_equal(dark[:3],np.zeros(3)),(light,physical,mode,dark)
            if mode==2: assert dark[3]==0,dark
assert np.any(sample((0,0,2),(.3,0,1,-1),2)>0)
wrapped=sample((2,0,-.2),(.3,0,1,-1),2)
assert np.any(wrapped[:3]>0) and wrapped[3]==0,wrapped

print('[RelightQ] legacy/PBR horizon PASS fully hidden spheres exactly zero; nonzero LUT',flush=True)
checks=0
for light in [(0,0,0),(0,0,-.1),(0,0,-2),(0,0,2),(1,0,2),(2,0,-.2),(0,0,15)]:
    for radius in [.005,.3,2.0]:
        for physical in [0,1]:
            settings=(radius,physical,128/9 if physical else 1,-1)
            for mode in [1,2]:
                a=sample(light,settings,mode);b=sample(light,settings,mode)
                assert a.tobytes()==b.tobytes(),(light,settings,mode,a,b)
                checks+=1
print('[RelightQ] calibration PASS GPU at d_ref within 1%',flush=True)
print('[RelightQ] shading maths PASS GPU near cap, wrap, representative point, finite inside/antiparallel',flush=True)
print('[RelightQ] determinism PASS',checks,'repeated helper readbacks at fixed 1x1 resolution',flush=True)

# Exercise the actual production forward evaluator, linked with its production
# utility modules, while keeping one executable and deliberately stale shapes.
runtime_wrapper="""
uniform vec3 testLight;
uniform vec4 testShape;
out vec4 result;
vec3 pbrCalcPointLightOrSpotLightRimKeepRef(vec3 diffuseColor, vec3 specularColor,
    float perceptualRoughness, float metallic, vec3 n, vec3 p, vec3 v,
    vec3 lp, vec3 ld, vec3 lightColor, float lightSize, float falloff,
    float is_pointlight, float ambiance, vec4 shape, vec4 rim, float spec_keep);
void main()
{
    vec3 color=pbrCalcPointLightOrSpotLightRimKeepRef(vec3(.6),vec3(.04),.2,0.0,
        vec3(0,0,1),vec3(0),vec3(0,0,1),testLight,vec3(0,0,-1),vec3(1),
        15.0,.5,1.0,0.0,testShape,vec4(0),1.0);
    result=vec4(color,1);
}
"""
pr=link_production('class2/deferred/pbralphaF.glsl','','runtime master readback',runtime_wrapper)
use(pr)
master=loc(pr,b'relight_enabled'); assert master >= 0
uniform2(loc(pr,b'relight_limits'),.1,4.0)
ui(loc(pr,b'rig_rim_mode'),0)
runtime_checks=0
for light in [(0,0,2),(1,0,2),(2,0,-.2),(0,0,-2)]:
    for physical in [0,1]:
        shaped=(.3,physical,128/9 if physical else 1,-1)
        ui(master,1)
        legacy=sample(light,(0,0,1,-1),1)
        on=sample(light,shaped,1)
        # Only the master changes: the nonzero shape is left in GPU storage.
        ui(master,0)
        off=sample(light,shaped,1)
        assert off.tobytes()==legacy.tobytes(),(light,physical,off,legacy)
        # Dependent limit changes cannot affect the disabled lighting path.
        uniform2(loc(pr,b'relight_limits'),10.0,0.0)
        assert sample(light,shaped,1).tobytes()==off.tobytes()
        uniform2(loc(pr,b'relight_limits'),.1,4.0)
        ui(master,1)
        assert sample(light,shaped,1).tobytes()==on.tobytes()
        if light==(2,0,-.2): assert np.any(on[:3]>off[:3]),(on,off)
        runtime_checks+=1
print('[RelightQ] runtime master PASS',runtime_checks,
      'production forward readbacks: on/off/on, stale shapes, exact legacy and disabled limits; no relink',flush=True)

# Rig-key regression uses this same WGL context, production linker and target.
exec(compile(Path(__file__).with_name('relight_shape_gl_check.py').read_text(),
             'relight_shape_gl_check.py', 'exec'))
if '--shape-only' in sys.argv:
    raise SystemExit(0)
exec(compile(Path(__file__).with_name('relight_rig_check.py').read_text(),
             'relight_rig_check.py', 'exec'))

if not shader_ref:
    exec(compile(Path(__file__).with_name('relight_area_check.py').read_text(),
                 'relight_area_check.py', 'exec'))
    exec(compile(Path(__file__).with_name('relight_world_check.py').read_text(),
                 'relight_world_check.py', 'exec'))
