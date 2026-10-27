#!/usr/bin/env python3
"""Environment-only production GLSL: isolation, tagged OFF bytes and 4K timers.

No viewer build/launch. --assemble-only never opens GL. --gpu checks the actual
process list before WGL. All shader bodies come from HEAD/WORKTREE; only vertex
geometry, textures, light/probe records and uniforms are fixture inputs.
"""
import argparse
import ast
import ctypes as C
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
import re

import numpy as np
import relight_3d_gl as G
from env_ev_math_check import fixture_factors
# Baseline = the commit this feature was built on (pre Environment only); not HEAD, which
# includes the feature once committed/merged. Override with env var.
import os as _os_base
BASE_REF = _os_base.environ.get('ENV_ONLY_GL_CHECK_BASE', '3901137b430')
# Environment EV rows compare against the tree that already had Environment only.
EV_BASE_REF = _os_base.environ.get('ENV_ONLY_GL_CHECK_EV_BASE', 'b8a911596c5')

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / '.tmp/env_only/gl'
FILES = dict(soften='class3/deferred/softenLightF.glsl', point='class3/deferred/pointLightF.glsl',
             spot='class3/deferred/spotLightF.glsl', multi='class3/deferred/multiPointLightF.glsl',
             alpha='class2/deferred/alphaF.glsl', pbr='class2/deferred/pbralphaF.glsl',
             material='class3/deferred/materialF.glsl', actor='class2/deferred/sharedActorFxPbrF.glsl',
             shiny='class3/deferred/fullbrightShinyF.glsl')
FORWARD = ('alpha', 'pbr', 'material', 'actor', 'shiny')
COMMON = ['deferred/globalF.glsl', 'environment/srgbF.glsl', 'windlight/atmosphericsVarsF.glsl',
          'windlight/gammaF.glsl', 'windlight/atmosphericsFuncs.glsl', 'windlight/atmosphericsF.glsl',
          'deferred/deferredUtil.glsl', 'deferred/gbufferUtil.glsl', 'windlight/atmosphericsHelpersF.glsl',
          'windlight/cineHazeF.glsl', 'environment/waterFogF.glsl', 'deferred/shadowUtil.glsl']
LEVEL1 = {'deferred/globalF.glsl', 'deferred/deferredUtil.glsl', 'deferred/gbufferUtil.glsl', 'deferred/shadowUtil.glsl'}


def source(ref, path):
    if ref == 'WORKTREE': return (ROOT / path).read_text(encoding='utf-8')
    return subprocess.check_output(['git', 'show', f'{ref}:{path}'], cwd=ROOT, timeout=20).decode('utf-8')


def tag_preamble(ref):
    text=source(ref,'indra/llrender/llshadermgr.cpp')
    lines=[json.loads('"'+match+'"') for match in re.findall(r'strdup\("([^"\n]*#define[^"\n]*)"\)',text)]
    selected=[line for line in lines if line.startswith(('#define GBUFFER_AVATAR_TAG 1','#define GBUFFER_UNTAG(', '#define GBUFFER_AVATAR_TAG_OF('))]
    selected.append([line for line in lines if line.startswith('#define GET_GBUFFER_FLAG')][-1])
    assert len(selected)==4,'production tag preamble changed'
    return '#undef GET_GBUFFER_FLAG\n'+''.join(selected)


def viewer_closed():
    result = subprocess.run(['powershell', '-NoProfile', '-Command',
        "$ErrorActionPreference='Stop'; $p=@(Get-Process -Name AlchemyTest -ErrorAction SilentlyContinue); Write-Output $p.Count"],
        capture_output=True, text=True, timeout=15)
    if result.returncode or result.stdout.strip() != '0':
        print('PENDING-GPU: viewer closure not established:', result.stdout, result.stderr)
        return False
    print('GPU lifecycle: PowerShell Get-Process AlchemyTest count=0')
    return True


def parity_class(namespace):
    # Import only the reusable class, NOT that module's top-level GL context.
    text = (HERE / 'alpha_blend_parity_gl_check.py').read_text(encoding='utf-8')
    node = next(n for n in ast.parse(text).body if isinstance(n, ast.ClassDef) and n.name == 'Runner')
    exec(compile(ast.get_source_segment(text, node), 'production_parity_runner', 'exec'), namespace)
    return namespace['Runner']


def run(args):
    global OUT
    OUT = args.out.resolve(); OUT.mkdir(parents=True, exist_ok=True)
    # Actual C++ factors are the GPU inputs; the independent summed-EV render
    # below is the reference. This CPU control also fails before implementation.
    environment_fixtures = fixture_factors(args.factor_source_ref)
    for row in environment_fixtures:
        expected = np.exp2(np.float32(.5 + (row['ev'] if row['enabled'] else 0)))
        for key in ('sun', 'ambient', 'diffuse', 'reflection', 'local', 'ssao'):
            assert np.isclose(row[key], expected, rtol=1.e-7), ('Environment EV summed factor', row, key, expected)
    print('PASS Environment EV CPU fixtures: six summed factors, OFF and zero controls', flush=True)
    inventory = {ref: {name: hashlib.sha256(source(ref, G.SHADERS+'/'+path).encode()).hexdigest()
                      for name, path in FILES.items()} for ref in dict.fromkeys((BASE_REF, EV_BASE_REF, 'WORKTREE'))}
    inventory['class3_probes'] = hashlib.sha256(source('WORKTREE', G.SHADERS+'/class3/deferred/reflectionProbeF.glsl').encode()).hexdigest()
    (OUT/'inventory.json').write_text(json.dumps(inventory, indent=2), encoding='utf-8')
    if args.assemble_only:
        print('PASS source inventory: nine production fragment mains + class3 reflection probes; GL unopened')
        return 0
    if not viewer_closed(): return 3
    gl = G.GL(); print('GL', gl.renderer, flush=True)
    err = gl.fn('glGetError', C.c_uint)
    enable = gl.fn('glEnable', None, C.c_uint)
    clear = gl.fn('glClear', None, C.c_uint)
    clearcolor = gl.fn('glClearColor', None, *([C.c_float]*4))
    bindout = gl.fn('glBindFragDataLocation', None, C.c_uint, C.c_uint, C.c_char_p)
    for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44): gl.disable(cap)
    width, height = 32, 16
    textures = []

    def tex(values, w=1, h=1, internal=0x8814):
        t=C.c_uint(); gl.gen_tex(1,C.byref(t)); textures.append(t)
        gl.bind_tex(0x0DE1,t)
        for p,v in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)): gl.param(0x0DE1,p,v)
        a=np.ascontiguousarray(values,dtype=np.float32)
        gl.teximage(0x0DE1,0,internal,w,h,0,0x1908,0x1406,a.ctypes.data)
        return t

    target=tex(np.zeros((height,width,4),np.float32),width,height)
    gbuf=[tex((.5,.45,.4,0)),tex((1,.45,.2,.6)),tex((.5,.5,0,.67)),tex((0,0,0,0))]
    depth=tex((.5,.5,.5,1)); light=tex((.2,.35,1,1)); lut=tex((.25,.25,.25,1))
    diffuse=tex((.8,.82,.85,.6)); flat_normal=tex((.5,.5,1,1)); orm=tex((1,.5,0,1)); black=tex((0,0,0,0))
    fbo=C.c_uint(); gl.gen_fbo(1,C.byref(fbo)); gl.bind_fbo(0x8D40,fbo)
    gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,target,0)
    assert gl.fbo_status(0x8D40)==0x8CD5
    gl.viewport(0,0,width,height)
    ns=dict(G=G,gl=gl,C=C,np=np,Path=Path,OUT=OUT,err=err,bindout=bindout,
        diffuse=diffuse,gbuf=gbuf,depth=depth,light=light,lut=lut,flat_normal=flat_normal,orm=orm,black=black)
    Base=parity_class(ns)

    class Runner(Base):
        def __init__(self,ref,tag,probes=2):
            super().__init__(ref); self.tag=tag; self.probes=probes; self.hashes={}; self.link_ms=[]; self.objects={}; self.tag_code=tag_preamble(ref)
        def source(self,name):
            if name in self.srccache:return self.srccache[name]
            if name.startswith('class'): path=G.SHADERS+'/'+name
            else:
                limit=1 if name in LEVEL1 else self.probes if name=='deferred/reflectionProbeF.glsl' else 3
                path=next(G.SHADERS+f'/class{i}/'+name for i in range(limit,0,-1)
                          if (ROOT/(G.SHADERS+f'/class{i}/'+name)).exists())
            s=source(self.ref,path); self.hashes[path]=hashlib.sha256(s.encode()).hexdigest()
            self.srccache[name]=s
            return s
        def compile(self,s,defs,label,stage=0x8B30):
            stage_define='#define FRAGMENT_SHADER 1\n' if stage==0x8B30 else '#define VERTEX_SHADER 1\n'
            key=s,defs,stage
            if key not in self.objects:self.objects[key]=super().compile(s,defs+stage_define+(self.tag_code if self.tag else ''),label,stage)
            return self.objects[key]
        def program(self,kind,production_vertex=False,weather=False):
            key=kind,production_vertex,weather
            if key in self.cache:return self.cache[key]
            defs='#define REFMAP_LEVEL 1\n#define LIGHT_COUNT '+('16' if kind=='multi' else '4')+'\n'
            if kind=='soften':defs+='#define HAS_SSAO 1\n#define HAS_SUN_SHADOW 1\n'
            if kind=='spot':defs+='#define SPOT_SHADOW 1\n'
            if kind=='alpha':defs+='#define USE_DIFFUSE_TEX 1\n#define USE_VERTEX_COLOR 1\n'
            if kind in ('material','pbr','actor'):defs+='#define DIFFUSE_ALPHA_MODE 1\n'
            if kind=='actor':defs+='#define SHARED_ACTOR_FX_REPLAY 1\n#define SHARED_ACTOR_FX_ALPHA_MODE 2\n#define HAS_ACTOR_FX 1\n'
            if weather:defs+='#define HAS_WEATHER_MATERIAL 1\n'
            mods=list(COMMON)
            if kind in FORWARD or kind=='soften': mods+=['deferred/reflectionProbeF.glsl','deferred/screenSpaceReflUtil.glsl']
            if kind=='actor':mods+=['alchemy/avatarLightResponseF.glsl','alchemy/actorFxDissolveF.glsl','alchemy/actorFxF.glsl']
            if weather:mods+=['deferred/weatherMaterialF.glsl','deferred/weatherShelterF.glsl']
            pr=gl.program()
            for m in mods:gl.attach(pr,self.compile(self.source(m),defs,m))
            gl.attach(pr,self.compile(self.source(FILES[kind]),defs,kind))
            if production_vertex:
                name=Path(FILES[kind]).name.replace('F.glsl','V.glsl')
                if kind=='spot':name='pointLightV.glsl'
                vertices=['deferred/'+name,'deferred/textureUtilV.glsl','objects/nonindexedTextureV.glsl']
                if kind in FORWARD or kind=='soften':
                    vertices+=['environment/srgbF.glsl','windlight/atmosphericsVarsV.glsl',
                        'windlight/atmosphericsHelpersV.glsl','windlight/atmosphericsFuncs.glsl','windlight/atmosphericsV.glsl']
                for m in vertices:gl.attach(pr,self.compile(self.source(m),defs,m,0x8B31))
            else:
                vary=('out vec2 vary_fragcoord;' if kind=='soften' else 'out vec4 vary_fragcoord; out vec3 trans_center;' if kind in ('point','spot','multi') else 'out vec3 vary_fragcoord;')
                tex1='out vec3 vary_texcoord1;' if kind=='shiny' else 'out vec2 vary_texcoord1;'
                v='''uniform vec3 fixtureNorm; uniform vec3 fixturePos; uniform vec4 fixtureColor; uniform vec3 fixtureLight;
out vec3 vary_position; out vec3 vary_normal; out vec3 vary_norm;
out vec2 vary_texcoord0; out vec4 vertex_color; out vec3 vary_actor_fx_position;
out vec2 base_color_texcoord; out vec2 normal_texcoord; out vec2 metallic_roughness_texcoord; out vec2 emissive_texcoord;
out vec3 vary_tangent; flat out float vary_sign;
'''+vary+tex1+'''
void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);
vary_position=fixturePos;vary_normal=fixtureNorm;vary_norm=fixtureNorm;vary_texcoord0=vec2(.5);
vertex_color=fixtureColor;vary_actor_fx_position=fixturePos;vary_tangent=vec3(1,0,0);vary_sign=1.;
base_color_texcoord=normal_texcoord=metallic_roughness_texcoord=emissive_texcoord=vec2(.5);
'''
                v+=('vary_texcoord1=fixtureNorm;' if kind=='shiny' else 'vary_texcoord1=vec2(.5);')
                v+=('vary_fragcoord=p;' if kind=='soften' else 'vary_fragcoord=gl_Position;trans_center=fixtureLight;' if kind in ('point','spot','multi') else 'vary_fragcoord=vec3(p,1);')+'}'
                gl.attach(pr,self.compile(v,defs,'fixture geometry',0x8B31))
            bindout(pr,0,b'frag_color')
            start=time.perf_counter(); gl.link(pr); ok=C.c_int();gl.getprog(pr,0x8B82,C.byref(ok)); self.link_ms.append((time.perf_counter()-start)*1000)
            if not ok.value:
                log=C.create_string_buffer(64000);gl.plog(pr,len(log),None,log);raise AssertionError(f'{self.ref}/{kind}/{production_vertex} LINK '+log.value.decode())
            self.cache[key]=pr;self.kinds[pr]=kind
            return pr

    runners={name:Runner(ref,tag,probe) for name,ref,tag,probe in (
        ('head',BASE_REF,False,2),('head_tag',BASE_REF,True,2),('ev_base',EV_BASE_REF,True,2),('off','WORKTREE',True,2),('probes','WORKTREE',True,3))}
    programs={}
    for label,r in runners.items():
        kinds=FILES if label!='probes' else ('soften','pbr','shiny')
        for kind in kinds:
            programs[label,kind]=r.program(kind)
            if label in ('head','off'):r.program(kind,True)
        print('PASS production linkage',label,len(kinds),'fragment mains',flush=True)

    # A real cube-array + populated std140 block ensures the class3 factor
    # checks exercise nonzero irradiance and radiance (never a zero-count stub).
    cube_textures=[]
    tex3=gl.fn('glTexImage3D',None,C.c_uint,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint,C.c_uint,C.c_void_p)
    for unit,color in ((13,(.3,.4,.6,1)),(14,(.6,.3,.2,1))):
        t=C.c_uint();gl.gen_tex(1,C.byref(t));cube_textures.append(t);gl.active(0x84C0+unit);gl.bind_tex(0x9009,t)
        for p,v in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F),(0x8072,0x812F)):gl.param(0x9009,p,v)
        data=np.tile(np.array(color,np.float32),(6,1));tex3(0x9009,0,0x8814,1,1,6,0,0x1908,0x1406,data.ctypes.data)
    environment=C.c_uint();gl.gen_tex(1,C.byref(environment));gl.active(0x84C0+15);gl.bind_tex(0x8513,environment)
    for p,v in ((0x2801,0x2600),(0x2800,0x2600)):gl.param(0x8513,p,v)
    cube_color=np.array((.25,.4,.6,1),np.float32)
    for face in range(6):gl.teximage(0x8515+face,0,0x8814,1,1,0,0x1908,0x1406,cube_color.ctypes.data)
    gl.active(0x84C0+16);brdf=tex((.5,.1,0,1))
    getblock=gl.fn('glGetUniformBlockIndex',C.c_uint,C.c_uint,C.c_char_p)
    blockbind=gl.fn('glUniformBlockBinding',None,C.c_uint,C.c_uint,C.c_uint)
    bindbase=gl.fn('glBindBufferBase',None,C.c_uint,C.c_uint,C.c_uint)
    # std140 offsets: refBox[256], heroBox, refSphere[256], refParams[256],
    # heroSphere, refIndex[256], refNeighbor[1024], refBucket[256], 4 ints.
    data=np.zeros(49248//4,np.float32)
    data[16448//4:16448//4+4]=(0,0,-2,100)
    data[20544//4:20544//4+4]=(1,1,1,.1)
    ints=data.view(np.int32);ints[24656//4:24656//4+4]=(0,-1,0,0);ints[49232//4]=1
    ubo=C.c_uint();gl.gen_buf(1,C.byref(ubo));gl.bind_buf(0x8A11,ubo);gl.buffer_data(0x8A11,data.nbytes,data.ctypes.data,0x88E4);bindbase(0x8A11,0,ubo)

    def normal_texture(tag,pbr=True):
        a=np.zeros((height,width,4),np.float32);a[...,:2]=.5;a[...,3]=.67 if pbr else .34
        if tag:a[:,:width//2,3]-=1/6
        gl.active(0x84C0+4);gl.bind_tex(0x0DE1,gbuf[2]);gl.teximage(0x0DE1,0,0x805B,width,height,0,0x1908,0x1406,a.ctypes.data)

    factors=dict(sun_pos=('sky_sun_ev_scale',(16.,)),sun_neg=('sky_sun_ev_scale',(.0625,)),
        sun_tint=('sky_sun_tint',(1.6,.6,.3)),ambient=('sky_amb_scale',(4.,)),
        ambient_tint=('sky_amb_tint',(.4,1.2,.8)),diffuse=('sky_gi_scale',(4.,)),
        reflection=('sky_probe_rad_scale',(4.,)),lift=('shadow_lift_gain',(3.,)),ssao=('ssao_irradiance_max',(.04,)),
        local_pos=('local_ev',(64.,)),local_neg=('local_ev',(.015625,)),size=('local_size',(.3,)))
    def setup(label,kind,mode,factor=None,pbr=True,preference=0,weather=False,env_row=None):
        r=runners[label];p=r.program(kind,weather=True) if weather else programs[label,kind];gl.use(p)
        r.defaults(p,local=.8,env=.5,spec=(.3,.3,.3,.6),coverage=.6,light_dir=(.995,0.,.1))
        r.set(p,'screen_res',float(width),float(height));r.set(p,'env_only_mode',mode);r.set(p,'gbuffer_tag_preferred',preference)
        r.set(p,'ssao_irradiance_max',.01);r.set(p,'ssao_irradiance_max_stock',.01);r.set(p,'local_light_ev_scale',1.)
        r.set(p,'shadow_lift_gain',0.);r.set(p,'local_light_size',0.)
        r.set(p,'reflection_probe_ambiance',.4);r.set(p,'falloff',.5);r.set(p,'far_z',-100.)
        r.set(p,'minimum_alpha',.01);r.set(p,'base_color_factor',1.,1.,1.,.6)
        r.set(p,'gobo_pattern',-1);r.set(p,'proj_shadow_idx',-1);r.set(p,'gobo_tint',1.,1.,1.)
        r.set(p,'proj_n',0.,0.,1.);r.set(p,'proj_p',0.,0.,-3.);r.set(p,'proj_range',15.);r.set(p,'proj_lod',1.)
        r.set(p,'gobo_anim_params',0.,0.,1.,0.)
        m=(C.c_float*16)(0,0,0,0,0,0,0,0,0,0,0,0,.5,.5,1,1);gl.matrix(gl.loc(p,b'proj_mat'),1,0,m)
        r.bind(p,'projectionMap',12,orm)
        r.bind(p,'brdfLut',16,brdf)
        gl.active(0x84C0+15);gl.bind_tex(0x8513,environment);r.set(p,'environmentMap',15)
        for i in range(16 if kind=='multi' else 4):
            r.set(p,f'light[{i}]',.02*i,0.,0.,6.);r.set(p,f'light_col[{i}]',.8,.8,.8,.5)
            r.set(p,f'local_light_ev_scales[{i}]',1.)
        for i in range(8):r.set(p,f'local_light_sizes[{i}]',0.)
        # Identity forward light set is a CPU input, exactly as in the viewer.
        if factor:
            key,values=factors[factor]
            if key=='local_ev':
                scale=1. if mode==2 else values[0]
                r.set(p,'color',.8*scale,.8*scale,.8*scale);r.set(p,'local_light_ev_scale',values[0])
                for i in range(16 if kind=='multi' else 4):r.set(p,f'light_col[{i}]',.8*scale,.8*scale,.8*scale,.5);r.set(p,f'local_light_ev_scales[{i}]',values[0])
                for i in range(8):r.set(p,f'light_diffuse[{i}]',*((.8*scale,)*3 if i==1 else (0.,)*3))
            elif key=='local_size':
                radius=0. if mode==2 else values[0];r.set(p,'local_light_size',radius)
                for i in range(8):r.set(p,f'local_light_sizes[{i}]',radius)
            else:
                r.set(p,key,*values)
        if env_row is not None:
            for name,key in (('sky_sun_ev_scale','sun'),('sky_amb_scale','ambient'),
                             ('sky_gi_scale','diffuse'),('sky_probe_rad_scale','reflection')):
                r.set(p,name,env_row[key])
            r.set(p,'ssao_irradiance_max',.01*env_row['ssao'])
            # Match the production forward identity light sets. Deferred
            # shaders receive the exposed radiance plus its combined inverse.
            local = env_row['local']
            scale = 1. if mode == 2 else local
            r.set(p,'color',.8*scale,.8*scale,.8*scale)
            r.set(p,'local_light_ev_scale',local)
            for i in range(16 if kind=='multi' else 4):
                r.set(p,f'light_col[{i}]',.8*scale,.8*scale,.8*scale,.5)
                r.set(p,f'local_light_ev_scales[{i}]',local)
            for i in range(8):r.set(p,f'light_diffuse[{i}]',*((.8*scale,)*3 if i==1 else (0.,)*3))
        if label=='probes':
            for name,unit in (('reflectionProbes',13),('irradianceProbes',14)):
                gl.active(0x84C0+unit);gl.bind_tex(0x9009,cube_textures[unit-13]);r.set(p,name,unit)
            block=getblock(p,b'ReflectionProbes')
            if block!=0xffffffff:blockbind(p,block,0)
        r.set(p,'weather_material_enabled',1 if weather else 0)
        r.set(p,'weather_material_excluded',0)
        r.set(p,'weather_material_wet',.8,0.,0.,0.)
        r.set(p,'weather_material_time_range',3.,100.)
        identity_matrix=np.eye(4,dtype=np.float32)
        gl.matrix(gl.loc(p,b'weather_material_view_to_agent'),1,0,identity_matrix.ctypes.data_as(C.POINTER(C.c_float)))
        cover=np.zeros((4,4),np.float32);cover[3,3]=1
        gl.matrix(gl.loc(p,b'weather_surface_cover_matrix'),1,0,cover.ctypes.data_as(C.POINTER(C.c_float)))
        r.bind(p,'weather_surface_cover_map',17,orm)
        # class2 fallback cube map must also be nonzero.
        normal_texture(label in ('head_tag','ev_base','off','probes'),pbr)
        gl.active(0x84C0);gl.bind_fbo(0x8D40,fbo);gl.viewport(0,0,width,height)
        clearcolor(0,0,0,0);clear(0x4000)
        assert err()==0,('setup',label,kind,factor)
        return p

    def render(*a,**kw):
        setup(*a,**kw);gl.draw(4,0,3)
        pixels=np.empty((height,width,4),np.float32);gl.read(0,0,width,height,0x1908,0x1406,pixels.ctypes.data)
        assert err()==0 and np.all(np.isfinite(pixels)),('read',a)
        return pixels

    results={}; fail=[]; positives=0
    def require(ok,name):
        if not ok:fail.append(name)
        print(('PASS ' if ok else 'FAIL ')+name,flush=True)
    for label,kinds in (('off',FILES),('probes',('soften','pbr','shiny'))):
        for kind in kinds:
            identity=render(label,kind,0)
            for row in environment_fixtures:
                summed=float(np.exp2(np.float32(.5+(row['ev'] if row['enabled'] else 0))))
                reference={key:summed for key in ('sun','ambient','diffuse','reflection','local','ssao')}
                adjusted=render(label,kind,0,env_row=reference)
                actual=render(label,kind,(2 if kind in FORWARD else 1) if row['enabled'] else 0,env_row=row)
                name=f'Environment EV/{label}/{kind}/{row["enabled"]}/{row["ev"]}'
                if row['enabled']:
                    area=actual if kind in FORWARD else actual[:,:width//2]
                    expected=identity if kind in FORWARD else identity[:,:width//2]
                    require(np.all(np.abs(area-expected)<=.001+.01*np.maximum(np.abs(area),np.abs(expected))),name+' avatar identity')
                    world=render(label,kind,0,env_row=row) if kind in FORWARD else actual[:,width//2:]
                    expected_set=adjusted if kind in FORWARD else adjusted[:,width//2:]
                    require(np.array_equal(world,expected_set),name+' set equals summed EV render')
                else:
                    require(np.array_equal(actual,adjusted),name+' OFF ignores master bytes')
                if row['ev']==0 or not row['enabled']:
                    baseline={key:float(np.exp2(np.float32(.5))) for key in reference}
                    old=render('ev_base' if label=='off' else label,kind,
                               (2 if kind in FORWARD else 1) if row['enabled'] else 0,env_row=baseline)
                    require(np.array_equal(actual,old),name+' HEAD/zero bytes')
    flag_outputs=[]
    for label in ('head_tag','off'):
        r=runners[label];p=gl.program()
        fragment='''uniform sampler2D flags;out vec4 frag_color;
void main(){float f=texelFetch(flags,ivec2(gl_FragCoord.xy),0).r;
frag_color=vec4(GET_GBUFFER_FLAG(f,GBUFFER_FLAG_SKIP_ATMOS)?1:0,GET_GBUFFER_FLAG(f,GBUFFER_FLAG_HAS_ATMOS)?1:0,
GET_GBUFFER_FLAG(f,GBUFFER_FLAG_HAS_PBR)?1:0,GET_GBUFFER_FLAG(f,GBUFFER_FLAG_HAS_HDRI)?1:0);}'''
        gl.attach(p,r.compile(fragment,'','seven flag codes'))
        gl.attach(p,r.compile('void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);}','','flag geometry',0x8B31))
        gl.link(p);ok=C.c_int();gl.getprog(p,0x8B82,C.byref(ok));assert ok.value
        values=np.zeros((1,7,4),np.float32);values[0,:,0]=(0,.34,.67,1,.34-1/6,.67-1/6,1-1/6)
        gl.active(0x84C0+18);flag_texture=tex(values,7,1,0x805B)
        gl.use(p);gl.ui(gl.loc(p,b'flags'),18);gl.viewport(0,0,7,1);gl.draw(4,0,3)
        readback=np.empty((1,7,4),np.float32);gl.read(0,0,7,1,0x1908,0x1406,readback.ctypes.data);flag_outputs.append(readback)
    require(np.array_equal(flag_outputs[0],flag_outputs[1]),'all seven RGBA16 codes x four flags: actual HEAD/current production preamble GL truth table')
    require(np.array_equal(flag_outputs[1][0],np.eye(4,dtype=np.float32)[[0,1,2,3,1,2,3]]),'flag truth table has no category cross-match')
    for kind in FILES:
        for pbr in (False,True) if kind not in FORWARD else (True,):
            for factor in factors:
                identity=render('off',kind,0,pbr=pbr)
                adjusted=render('off',kind,0,factor,pbr=pbr)
                on=render('off',kind,2 if kind in FORWARD else 1,factor,pbr=pbr)
                reference=render('head',kind,0,factor,pbr=pbr)
                again=render('off',kind,0,factor,pbr=pbr)
                name=f'{kind}/{pbr}/{factor}'
                require(np.array_equal(reference,adjusted),name+' HEAD/OFF bytes incl tagged avatars')
                require(np.array_equal(adjusted,again),name+' ON-to-OFF bytes')
                area=on if kind in FORWARD else on[:,:width//2]
                expected=identity if kind in FORWARD else identity[:,:width//2]
                require(np.all(np.abs(area-expected)<=.001+.01*np.maximum(np.abs(area),np.abs(expected))),name+' avatar identity')
                if kind not in FORWARD:require(np.array_equal(on[:,width//2:],adjusted[:,width//2:]),name+' set exact')
                delta=float(np.max(np.abs(adjusted-identity))); positives+=delta>.001
                results[name]=dict(response=delta)
    require(positives>=15,'nonvacuous factor responses across production callers')
    expected_consumers={
        'soften':('sun_pos','sun_neg','sun_tint','ambient','ambient_tint','diffuse','reflection','lift','ssao'),
        'point':('local_pos','local_neg','size'), 'spot':('local_pos','local_neg','size'),
        'multi':('local_pos','local_neg','size'),
        'alpha':('sun_pos','ambient','ambient_tint','diffuse','local_pos','local_neg','lift'),
        'pbr':('sun_pos','ambient','ambient_tint','diffuse','reflection','local_pos','local_neg','size','lift'),
        'material':('sun_pos','ambient','ambient_tint','diffuse','reflection','local_pos','local_neg','lift'),
        'actor':('sun_pos','ambient','ambient_tint','diffuse','reflection','local_pos','local_neg','size','lift'),
        'shiny':('reflection',)}
    for kind,consumers in expected_consumers.items():
        for factor in consumers:require(results[f'{kind}/True/{factor}']['response']>1.e-5,f'{kind}/{factor} consumed-factor positive control')
    for kind in ('soften','pbr','shiny'):
        # Fullbright shiny consumes only the radiance output; irradiance is
        # deliberately unused in its production main.
        for factor in (('reflection',) if kind=='shiny' else ('diffuse','reflection','ambient_tint')):
            identity=render('probes',kind,0);adjusted=render('probes',kind,0,factor)
            on=render('probes',kind,1 if kind=='soften' else 2,factor)
            area=slice(None,width//2) if kind=='soften' else slice(None)
            require(np.allclose(on[:,area],identity[:,area],atol=.001,rtol=.01),f'class3/{kind}/{factor} avatar probe identity')
            require(np.max(np.abs(adjusted-identity))>.001,f'class3/{kind}/{factor} populated-probe response')
    for kind in ('alpha','pbr','material','actor'):
        dry=render('off',kind,0)
        wet=render('off',kind,0,weather=True)
        require(np.max(np.abs(wet-dry))>1.e-5,f'weather/{kind} nonzero wet-material positive control')
        for preference,label in ((0,'head'),(1,'head_tag')):
            baseline=render(label,kind,0,'sun_pos',preference=preference,weather=True)
            current=render('off',kind,0,'sun_pos',preference=preference,weather=True)
            require(np.array_equal(current,baseline),f'weather/{kind}/preference{preference} HEAD/OFF bytes')
    # Genuine source-over blending of the production alpha shader over a wall.
    a0=render('off','alpha',0);a1=render('off','alpha',0,'sun_pos');a2=render('off','alpha',2,'sun_pos')
    wall0=np.array((.1,.2,.3));wall1=np.array((.8,.6,.4))
    enable(0x0BE2);gl.fn('glBlendFuncSeparate',None,C.c_uint,C.c_uint,C.c_uint,C.c_uint)(0x0302,0x0303,1,0x0303)
    setup('off','alpha',2,'sun_pos');clearcolor(*wall1,1.);clear(0x4000);gl.draw(4,0,3)
    composite=np.empty((height,width,4),np.float32);gl.read(0,0,width,height,0x1908,0x1406,composite.ctypes.data);gl.disable(0x0BE2)
    expected=a0[...,:3]*a0[...,3:4]+wall1*(1-a0[...,3:4])
    require(np.allclose(composite[...,:3],expected,atol=.001,rtol=.01),'forward hair identity + adjusted wall-through production blend')
    report=dict(renderer=gl.renderer,checks=results,failures=fail,startup={k:dict(programs=len(r.cache),link_ms=r.link_ms,source_hashes=r.hashes) for k,r in runners.items()})
    if args.cost:
        # Same linked production executables; 12 alpha draws represent a crowd.
        # Fixed half-tagged receiver field. No readback falls inside a timer.
        width,height=3840,2160
        gl.bind_tex(0x0DE1,target);gl.teximage(0x0DE1,0,0x8814,width,height,0,0x1908,0x1406,None)
        states={}
        for label in ('head','head_repeat','off','on'):
            row=[]
            for kind in ('soften','multi','alpha'):
                setup('head' if label.startswith('head') else 'off',kind,0 if label!='on' else 2 if kind=='alpha' else 1,'sun_pos')
                row.append(programs['head' if label.startswith('head') else 'off',kind])
            states[label]=row
        # Baseline and current shaders must read their respective flag encoding.
        # Do not let the last setup overwrite the HEAD timer's normal input.
        normal_inputs={}
        for label in states:
            a=np.zeros((height,width,4),np.float32);a[...,:2]=.5;a[...,3]=.67
            if not label.startswith('head'):a[:,:width//2,3]-=1/6
            gl.active(0x84C0+4);normal_inputs[label]=tex(a,width,height,0x805B)
        # Uniforms belong to programs, so OFF and ON share a program but switch
        # the mode explicitly just before its timed draw (no source rebuild).
        blend_func=gl.fn('glBlendFuncSeparate',None,C.c_uint,C.c_uint,C.c_uint,C.c_uint)
        scissor=gl.fn('glScissor',None,C.c_int,C.c_int,C.c_int,C.c_int)
        mode_locations={p:gl.loc(p,b'env_only_mode') for row in states.values() for p in row}
        # Twelve nonoverlapping cards cover the avatar half of the 4K field.
        # Twelve fullscreen layers would represent 12 screens of alpha rather
        # than a 12-avatar crowd. Local lighting still covers the entire frame.
        rectangles=[((i%4)*width//8,(i//4)*height//3,width//8,height//3) for i in range(12)]
        import itertools
        orders=list(itertools.permutations(states))
        variant_count=len(states)
        queries=(C.c_uint*(660*variant_count))();gl.gen_queries(len(queries),queries)
        queued=[]
        for iteration in range(660):
            order=orders[iteration%len(orders)]
            for j,label in enumerate(order):
                gl.active(0x84C0+4);gl.bind_tex(0x0DE1,normal_inputs[label])
                gl.begin_query(0x88BF,queries[iteration*variant_count+j])
                for kind,p in zip(('soften','multi','alpha'),states[label]):
                    gl.use(p);gl.ui(mode_locations[p],0 if label!='on' else 2 if kind=='alpha' else 1)
                    if kind=='soften':gl.disable(0x0BE2)
                    else:
                        enable(0x0BE2)
                        blend_func(*((1,1,1,1) if kind=='multi' else (0x0302,0x0303,1,0x0303)))
                    if kind=='alpha':
                        enable(0x0C11)
                        for rectangle in rectangles:scissor(*rectangle);gl.draw(4,0,3)
                        gl.disable(0x0C11)
                    else:
                        for draw_index in range(2 if kind=='multi' else 1):gl.draw(4,0,3)
                gl.end_query(0x88BF)
            queued.append(order)
        # Drain after queueing the run, keeping the GPU busy rather than
        # alternating tiny GPU bursts with blocking CPU readback/idle gaps.
        pairs=[]
        for iteration,order in enumerate(queued):
            values={}
            for j,label in enumerate(order):
                ns64=C.c_uint64();gl.query_u64(queries[iteration*variant_count+j],0x8866,C.byref(ns64));values[label]=ns64.value*1.e-6
            if iteration>=60:pairs.append(values)
        stats={label:float(np.percentile([r[label] for r in pairs],95)) for label in states}
        deltas={label:max(float(np.percentile([r[label]-r['head'] for r in pairs],95)),stats[label]-stats['head']) for label in ('off','on')}
        null_delta=float(np.percentile([abs(r['head_repeat']-r['head']) for r in pairs],95))
        report['cost']=dict(resolution=[width,height],alpha_draws=12,point_lights=32,raw_ms=pairs,p95_ms=stats,delta_ms=deltas,budgets_ms=dict(off=.10,on=.35),identical_head_p95_delta_ms=null_delta)
        require(null_delta<=.10,'4K identical-HEAD control resolves <= 0.10 ms (otherwise timing is INCONCLUSIVE)')
        require(deltas['off']<=.10,'4K OFF p95 <= 0.10 ms')
        require(deltas['on']<=.35,'4K ON p95 <= 0.35 ms')
    report['failures']=fail
    (OUT/'results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PASS' if not fail else 'FAIL',len(fail),'environment-only GL failures',flush=True)
    return bool(fail)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    group=ap.add_mutually_exclusive_group(required=True);group.add_argument('--assemble-only',action='store_true');group.add_argument('--gpu',action='store_true')
    ap.add_argument('--cost',action='store_true');ap.add_argument('--out',type=Path,default=OUT)
    ap.add_argument('--factor-source-ref',help='fail-before control for production CPU factors, e.g. HEAD')
    return run(ap.parse_args())


if __name__=='__main__':raise SystemExit(main())
