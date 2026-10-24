"""Real-WGL BLEND/MASK parity gate using production material and lighting shaders.

Run with PYTHONUTF8=1. No viewer build. Default: WORKTREE plus a required negative
control at 13028c968b4. --shader-ref 13028c968b4 must exit 1. --shader-root DIR
can instead read a scratch shader tree. Consumed sources and RGBA32F readbacks
are saved under --out. Missing GL or compilation failure is a failure.

The strand's encoded RGBA8 texture is shared by the forward shader and real
masked G-buffer writer, then production soften/point fragments light the latter.
Only vertex geometry, material inputs, atmospheric/light uniforms and neutral
textures are fixtures. No CPU lighting implementation or patched GLSL oracle.
Tolerances include SRGB8 G-buffer quantization (0.004 absolute RGB); PBR local
terms also require <=0.6% relative error. Two-sided cards test a lit backside
and a culled single-sided backside. Coverage is checked with real GL blending.
"""
import os
os.environ['PYTHONUTF8'] = '1'
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import sys, json, ctypes as C, argparse
from pathlib import Path
import numpy as np
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/alpha_port/parity_gl'
sys.path.insert(0, str(ROOT / 'scripts/code_tools'))
import relight_3d_gl as G

gl = G.GL()
err = gl.fn('glGetError', C.c_uint)
bindout = gl.fn('glBindFragDataLocation', None, C.c_uint, C.c_uint, C.c_char_p)
drawbuffers = gl.fn('glDrawBuffers', None, C.c_int, C.POINTER(C.c_uint))
readbuffer = gl.fn('glReadBuffer', None, C.c_uint)
enable = gl.fn('glEnable', None, C.c_uint)
clearcolor = gl.fn('glClearColor', None, *([C.c_float]*4))
clear = gl.fn('glClear', None, C.c_uint)
blend = gl.fn('glBlendFuncSeparate', None, *([C.c_uint]*4))
for cap in (0x0B71,0x0BE2,0x0C11,0x8DB9,0x0B44): gl.disable(cap)
gl.viewport(0,0,1,1)
fbo=C.c_uint(); gl.gen_fbo(1,C.byref(fbo)); gl.bind_fbo(0x8D40,fbo)

def tex(rgba, internal=0x8814):
    previous=C.c_int();gl.fn('glGetIntegerv',None,C.c_uint,C.POINTER(C.c_int))(0x8069,C.byref(previous))
    t=C.c_uint(); gl.gen_tex(1,C.byref(t)); gl.bind_tex(0x0DE1,t)
    for p,v in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)): gl.param(0x0DE1,p,v)
    a=np.array(rgba,np.float32)
    gl.teximage(0x0DE1,0,internal,1,1,0,0x1908,0x1406,a.ctypes.data)
    gl.bind_tex(0x0DE1,previous.value)
    return t

target=tex((0,0,0,0))
gbuf=[tex((0,0,0,0),0x8C43),tex((0,0,0,0)),tex((0,0,0,0)),tex((0,0,0,0))]
depth=tex((.5,.5,.5,1)); light=tex((1,1,1,1)); lut=tex((.25,.25,.25,1))
diffuse=tex((.8,.82,.85,1),0x8058)
flat_normal=tex((.5,.5,1.,1.))
orm=tex((1.,1.,0.,1.))
black=tex((0.,0.,0.,0.))

def target_bind(ids):
    for i in range(4): gl.fbo_tex(0x8D40,0x8CE0+i,0x0DE1,ids[i] if i<len(ids) else 0,0)
    bufs=(C.c_uint*len(ids))(*[0x8CE0+i for i in range(len(ids))]); drawbuffers(len(ids),bufs)
    assert gl.fbo_status(0x8D40)==0x8CD5
    readbuffer(0x8CE0)

def sample():
    p=np.empty(4,np.float32); gl.read(0,0,1,1,0x1908,0x1406,p.ctypes.data)
    assert np.all(np.isfinite(p)),p
    assert err()==0
    return p.tolist()

COMMON=['deferred/globalF.glsl','environment/srgbF.glsl','windlight/atmosphericsVarsF.glsl','windlight/gammaF.glsl','windlight/atmosphericsFuncs.glsl','windlight/atmosphericsF.glsl','deferred/deferredUtil.glsl','deferred/gbufferUtil.glsl','windlight/atmosphericsHelpersF.glsl','windlight/cineHazeF.glsl','environment/waterFogF.glsl','deferred/shadowUtil.glsl','deferred/reflectionProbeF.glsl','deferred/screenSpaceReflUtil.glsl']
LEVEL1={'deferred/globalF.glsl','deferred/deferredUtil.glsl','deferred/gbufferUtil.glsl','deferred/shadowUtil.glsl'}

class Runner:
    def __init__(self,ref,source_root=None):
        self.ref=ref
        self.tree=G.SourceTree(ref)
        self.source_root=source_root
        self.cache={}
        self.srccache={}
        self.kinds={}
    def source(self,name):
        if name in self.srccache:return self.srccache[name]
        if name.startswith('class'): p=G.SHADERS+'/'+name
        else:
            limit=1 if name in LEVEL1 else (2 if name=='deferred/reflectionProbeF.glsl' else 3)
            p=next(G.SHADERS+'/class%d/'%i+name for i in range(limit,0,-1) if ((self.source_root / ('class%d/'%i+name)).exists() if self.source_root else self.tree.exists(G.SHADERS+'/class%d/'%i+name)))
        s=(self.source_root / p.removeprefix(G.SHADERS+'/')).read_text(encoding='utf-8') if self.source_root else self.tree.read(p)
        dest=OUT/'sources'/self.ref/p
        dest.parent.mkdir(parents=True,exist_ok=True); dest.write_text(s,encoding='utf-8')
        # Consume the saved snapshot, including git-show baseline sources.
        s=dest.read_text(encoding='utf-8')
        self.srccache[name]=s
        return s
    def compile(self,s,defs,label,stage=0x8B30):
        sh=gl.create_shader(stage)
        injection=G.PREAMBLE+defs+'\n'
        s=s.replace('/*[EXTRA_CODE_HERE]*/',injection,1) if '/*[EXTRA_CODE_HERE]*/' in s else injection+s
        src=C.c_char_p(('#version 420\n'+s).encode()); gl.source(sh,1,C.byref(src),None); gl.compile(sh)
        ok=C.c_int(); gl.getshader(sh,0x8B81,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);gl.slog(sh,len(log),None,log);raise RuntimeError(label+' '+log.value.decode())
        return sh
    def program(self,kind,variant=''):
        key=kind,variant
        if key in self.cache:return self.cache[key]
        defs='#define REFMAP_LEVEL 1\n'
        defs+='#define USE_DIFFUSE_TEX 1\n#define USE_VERTEX_COLOR 1\n' if kind=='alpha' else ''
        defs+='#define DIFFUSE_ALPHA_MODE %d\n'%(1 if kind=='material' else 2) if kind.startswith('material') else ''
        defs+='#define HAS_ALPHA_MASK 1\n' if kind=='pbr_mask' else ''
        defs+='#define HAS_NORMAL_MAP 1\n' if variant=='normal_map' else ''
        defs+='#define HAS_SKIN 1\n' if variant=='skin' else ''
        defs+='#define HAS_SUN_SHADOW 1\n#define SUN_SHADOW 1\n' if variant=='shadow' or kind=='sunshadow' else ''
        file={'alpha':'class2/deferred/alphaF.glsl','mask':'deferred/diffuseAlphaMaskF.glsl','soften':'class3/deferred/softenLightF.glsl','material':'class3/deferred/materialF.glsl','material_mask':'class3/deferred/materialF.glsl','point':'class3/deferred/pointLightF.glsl','pbr':'class2/deferred/pbralphaF.glsl','pbr_mask':'class1/deferred/pbropaqueF.glsl','sunshadow':'class2/deferred/sunLightF.glsl','haze':'class3/deferred/hazeF.glsl'}[kind]
        s=self.source(file)
        if variant=='actor_neutral':
            defs+='#define HAS_ACTOR_FX 1\n'
        mods=list(COMMON)
        if kind=='point':mods=[m for m in mods if m not in ('deferred/reflectionProbeF.glsl','deferred/screenSpaceReflUtil.glsl')]
        if variant=='actor_neutral':mods+=['alchemy/avatarLightResponseF.glsl','alchemy/actorFxDissolveF.glsl','alchemy/actorFxF.glsl']
        pr=gl.program()
        for m in mods:gl.attach(pr,self.compile(self.source(m),defs,m))
        gl.attach(pr,self.compile(s,defs,kind))
        vary='out vec2 vary_fragcoord;' if kind in ('soften','sunshadow','haze') else 'out vec3 vary_fragcoord;'
        if kind=='point':vary='out vec4 vary_fragcoord; out vec3 trans_center;'
        v='''uniform int fixtureBack; uniform int fixtureGeometry; uniform vec3 fixtureNorm; uniform vec3 fixturePos; uniform vec4 fixtureColor; uniform vec3 fixtureLight;
        out vec3 vary_position; out vec3 vary_normal; out vec3 vary_norm;
        out vec2 vary_texcoord0; out vec2 vary_texcoord1;
        out vec3 vary_tangent; flat out float vary_sign;
        out vec2 base_color_texcoord; out vec2 normal_texcoord;
        out vec2 metallic_roughness_texcoord; out vec2 emissive_texcoord;
        out vec4 vertex_color; out vec3 vary_actor_fx_position;'''+vary+'''
        void main(){ int id=fixtureBack!=0 && gl_VertexID!=0 ? 3-gl_VertexID : gl_VertexID;
        vec2 p=vec2((id<<1)&2,id&2); gl_Position=vec4(p*2-1,0,1);
        vary_position=fixturePos;vary_normal=fixtureNorm;vary_norm=fixtureNorm;
        vary_texcoord0=vary_texcoord1=vec2(.5);
        vertex_color=fixtureColor;vary_actor_fx_position=fixturePos;
        vary_tangent=vec3(1,0,0);vary_sign=1.;
        base_color_texcoord=normal_texcoord=metallic_roughness_texcoord=emissive_texcoord=vec2(.5);
        if(fixtureGeometry!=0) {vary_norm=vec3(id==0?sqrt(.75):-sqrt(.75),0,.5*sign(fixtureNorm.z));vary_normal=vary_norm;}'''
        v+= 'vary_fragcoord=vec2(.5);' if kind in ('soften','sunshadow','haze') else ('vary_fragcoord=vec4(0,0,0,1);trans_center=fixtureLight;' if kind=='point' else 'vary_fragcoord=vec3(0,0,1);')
        v+='}'
        gl.attach(pr,self.compile(v,defs,'vertex',0x8B31))
        if kind in ('mask','material_mask','pbr_mask'):
            for i in range(4):bindout(pr,i,('frag_data[%d]'%i).encode())
        else:bindout(pr,0,b'frag_color')
        gl.link(pr);ok=C.c_int();gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);gl.plog(pr,len(log),None,log);raise RuntimeError(kind+' '+log.value.decode())
        self.cache[key]=pr
        self.kinds[pr]=kind
        return pr
    def set(self,p,k,*vals):
        l=gl.loc(p,k.encode())
        if len(vals)==1 and isinstance(vals[0],int):gl.ui(l,vals[0])
        else:gl.uf[len(vals)](l,*vals)
    def bind(self,p,k,unit,t):
        gl.active(0x84C0+unit);gl.bind_tex(0x0DE1,t);gl.ui(gl.loc(p,k.encode()),unit)
    def defaults(self,p,classic=0,nlen=1.,sun=(.7,.65,.6),ambient=(.4,.45,.5),local=0.,shape=(0,0,1,-1),spec=(0,0,0,0),env=0.,local_distance=2.,light_falloff=.5,back=False,geometry=False,coverage=1.):
        gl.use(p)
        sets=dict(fixtureNorm=(0.,0.,nlen),fixturePos=(0.,0.,-2.),fixtureColor=(1.,1.,1.,coverage),fixtureBack=(int(back),),fixtureGeometry=(int(geometry),),roughnessFactor=(.5,),metallicFactor=(0.,),fixtureLight=(0.,0.,0.),sun_dir=(0.,0.,1.),moon_dir=(0.,0.,1.),sun_up_factor=(1,),classic_mode=(classic,),lightnorm=(0.,1.,0.),sunlight_color=sun,moonlight_color=sun,ambient_color=ambient,sky_sunlight_scale=(1.,),sky_ambient_scale=(1.,),sky_sun_ev_scale=(1.,),sky_amb_scale=(1.,),sky_sun_tint=(1.,1.,1.),sky_amb_tint=(1.,1.,1.),sky_gi_scale=(1.,),sky_probe_rad_scale=(1.,),sky_hdr_scale=(1.,),max_y=(10000.,),glow=(1.,0.,-1.),scene_light_strength=(1.,),waterPlane=(0.,0.,0.,1.),waterSign=(1.,),screen_res=(1.,1.),minimum_alpha=(.33,),specular_color=spec,env_intensity=(env,),relight_enabled=(1,),relight_limits=(.1,100.),relight_highlight_scale=(1.,),relight_legacy_exponent=(368.,),ssao_irradiance_scale=(.6,),ssao_irradiance_max=(.18,),size=(6.,),falloff=(.5,),color=(local,local,local))
        for k,v in sets.items():self.set(p,k,*v)
        self.set(p,'fixtureLight',0.,0.,-2.+local_distance)
        self.set(p,'falloff',light_falloff)
        self.set(p,'relight_limits',.1,4.)
        inv=np.eye(4,dtype=np.float32);inv[2,3]=-2
        gl.matrix(gl.loc(p,b'inv_proj'),1,0,np.ascontiguousarray(inv.T).ctypes.data_as(C.POINTER(C.c_float)))
        m=np.eye(3,dtype=np.float32);m3=gl.fn('glUniformMatrix3fv',None,C.c_int,C.c_int,C.c_uint,C.POINTER(C.c_float))
        for k in ('env_mat','ssao_effect_mat'):
            if k=='env_mat' and self.kinds.get(p)=='point':continue
            m3(gl.loc(p,k.encode()),1,0,m.ctypes.data_as(C.POINTER(C.c_float)))
        assert err()==0, 'fixture uniforms'
        for i in range(8):
            self.set(p,'light_position[%d]'%i,0.,0.,-2.+local_distance,1.)
            self.set(p,'light_direction[%d]'%i,0.,0.,-1.)
            self.set(p,'light_attenuation[%d]'%i,(3.+12.*light_falloff)/6.,1.+light_falloff,1.,0.)
            self.set(p,'light_diffuse[%d]'%i,*((local,)*3 if i==1 else (0.,)*3))
            self.set(p,'light_shapes[%d]'%i,*shape)
            self.set(p,'light_deferred_attenuation[%d]'%i,6.,light_falloff)
        self.set(p,'light_shape',*shape)
        for k,u,t in [('diffuseMap',1,diffuse),('diffuseRect',2,gbuf[0]),('specularRect',3,gbuf[1]),('normalMap',4,gbuf[2]),('emissiveRect',5,gbuf[3]),('depthMap',6,depth),('lightMap',7,light),('lightFunc',8,lut),('bumpMap',9,flat_normal),('specularMap',10,orm),('emissiveMap',11,black)]:self.bind(p,k,u,t)
    def draw(self,kind,variant='',**kw):
        p=self.program(kind,variant);self.defaults(p,**kw)
        target_bind(gbuf if kind in ('mask','material_mask','pbr_mask') else [target])
        clearcolor(-5.,-5.,-5.,-5.);clear(0x4000)
        # PBR writes linear albedo; legacy writers publish encoded albedo.
        # Mirror the viewer's per-pool framebuffer conversion state.
        if kind == 'pbr_mask': enable(0x8DB9)
        gl.draw(4,0,3)
        gl.disable(0x8DB9)
        return sample()


def measure(r):
    global diffuse
    rows = {}
    dark = dict(sun=(0., 0., 0.), ambient=(0., 0., 0.))

    def pair(label, kind='alpha', include_local=False, variant='', **kw):
        mask = {'alpha': 'mask', 'material': 'material_mask', 'pbr': 'pbr_mask'}[kind]
        r.draw(mask, variant, **kw)
        deferred = np.array(r.draw('soften', **kw))
        if include_local:
            deferred[:3] += r.draw('point', **kw)[:3]
        forward = r.draw(kind, variant, **kw)
        rows[label] = dict(blend=forward, masked_deferred=deferred.tolist(),
                           delta_rgb=(np.array(forward[:3])-deferred[:3]).tolist())
        return forward, deferred.tolist()

    # Unit normals at the three vertices interpolate to length .5 at this texel.
    pair('sun', geometry=True, ambient=(0., 0., 0.))
    pair('sky', geometry=True, sun=(0., 0., 0.))
    pair('combined', geometry=True)
    pair('classic', geometry=True, classic=1)
    pair('skin_sun', variant='skin', geometry=True, ambient=(0., 0., 0.))
    pair('material_mapped_front', kind='material', variant='normal_map')
    pair('material_mapped_back', kind='material', variant='normal_map', nlen=-1., back=True)
    for kind in ('alpha', 'material', 'pbr'):
        pair(kind+'_front', kind=kind)
        pair(kind+'_back', kind=kind, nlen=-1., back=True)
        for name, shape, distance, falloff in (
                ('point', (0., 0., 1., -1.), .25, 0.),
                ('shaped', (.45, 0., 1., -1.), 2., .5),
                ('physical', (.45, 1., 2.2755555556, -1.), .25, .5)):
            pair(kind+'_'+name, kind=kind, include_local=True, local_distance=distance,
                 light_falloff=falloff, shape=shape, local=.5, **dark)

        # Culling remains the double-sided control, just as in the viewer.
        enable(0x0B44)
        rows[kind+'_culled_back'] = r.draw(kind, nlen=-1., back=True)
        gl.disable(0x0B44)

        # Partial-alpha content uses the same lighting and straight coverage.
        kw = dict(local=.5, shape=(.45, 1., 2.2755555556, -1.),
                  local_distance=.25, **dark)
        lit = r.draw(kind, **kw)
        p = r.program(kind)
        r.defaults(p, coverage=.6, **kw)
        target_bind([target])
        background = np.array([.02, .03, .04])
        clearcolor(*background, 0.)
        clear(0x4000)
        enable(0x0BE2)
        blend(0x0302, 0x0303, 0, 0x0303)
        gl.draw(4, 0, 3)
        gl.disable(0x0BE2)
        composed = sample()
        rows[kind+'_coverage'] = dict(lit=lit, composed=composed,
            expected_rgb=(.6*np.array(lit[:3])+.4*background).tolist())
        rows[kind+'_actor_neutral'] = r.draw(kind, 'actor_neutral')

    strand = diffuse
    diffuse = tex((.8, .82, .85, .01), 0x8058)
    rows['glass_glint'] = r.draw('material', spec=(.5, .5, .5, .8),
                                 ambient=(0., 0., 0.))
    diffuse = strand

    # The production reference helper is separate from the split helper used by
    # pbralphaF. Execute both to guard all four intensity sites, including the
    # shared Actor FX reference path. This wrapper only supplies material inputs.
    wrapper = '''
uniform sampler2D diffuseMap;
uniform vec3 fixturePos, fixtureLight;
uniform vec4 fixtureShape;
uniform float falloff;
out vec4 frag_color;
void main() {
    vec3 base = srgb_to_linear(texture(diffuseMap, vec2(.5)).rgb);
    vec3 d = vec3(0), s = vec3(0);
    calcDiffuseSpecular(base, 0., d, s);
    vec3 rgb = pbrCalcPointLightOrSpotLightRimKeepRef(d, s, .5, 0.,
        vec3(0,0,1), fixturePos, -normalize(fixturePos), fixtureLight,
        vec3(0,0,-1), vec3(.5), 6., falloff, 1., 0., fixtureShape, vec4(0), 1.);
    frag_color = vec4(rgb, 1.);
}
'''
    pr = gl.program()
    for module in COMMON:
        source = r.source(module)
        if module == 'deferred/deferredUtil.glsl': source += wrapper
        gl.attach(pr, r.compile(source, '', module))
    vertex = '''void main() {vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
        gl_Position=vec4(p*2-1,0,1);}'''
    gl.attach(pr, r.compile(vertex, '', 'reference vertex', 0x8B31))
    gl.link(pr)
    ok = C.c_int()
    gl.getprog(pr, 0x8B82, C.byref(ok))
    if not ok.value:
        log = C.create_string_buffer(64000)
        gl.plog(pr, len(log), None, log)
        raise RuntimeError(log.value.decode())
    for name, shape, distance, falloff in (
            ('point', (0., 0., 1., -1.), .25, 0.),
            ('shaped', (.45, 0., 1., -1.), 2., .5),
            ('physical', (.45, 1., 2.2755555556, -1.), .25, .5)):
        r.defaults(pr, local_distance=distance, light_falloff=falloff, **dark)
        r.set(pr, 'fixtureShape', *shape)
        target_bind([target])
        gl.draw(4, 0, 3)
        rows['pbr_reference_'+name] = sample()
    return rows


def validate(rows):
    checks = {}
    for name, row in rows.items():
        if 'delta_rgb' in row:
            delta = np.abs(row['delta_rgb'])
            # Quantization in the actual sRGB G-buffer is the residual oracle error.
            checks[name] = bool(np.max(delta) <= .004)
            if name.startswith('pbr_') and name.split('_')[-1] in ('point', 'shaped', 'physical'):
                ref = np.maximum(np.abs(row['masked_deferred'][:3]), .001)
                checks[name+'_relative'] = bool(np.max(delta/ref) <= .006)
    for kind in ('alpha', 'material', 'pbr'):
        front, back = rows[kind+'_front'], rows[kind+'_back']
        for path in ('blend', 'masked_deferred'):
            checks[kind+'_back_facing_'+path] = bool(np.allclose(
                front[path][:3], back[path][:3], atol=.004, rtol=0))
        checks[kind+'_single_sided'] = rows[kind+'_culled_back'] == [-5.]*4
        row = rows[kind+'_coverage']
        checks[kind+'_coverage'] = bool(np.allclose(
            row['composed'][:3], row['expected_rgb'], atol=1.e-6, rtol=0))
        checks[kind+'_coverage_alpha'] = abs(row['lit'][3]-1.) < 1.e-6
        checks[kind+'_actor_neutral'] = bool(np.allclose(
            rows[kind+'_actor_neutral'], front['blend'], atol=1.e-6, rtol=0))
    checks['glass_sun_glint_alpha'] = rows['glass_glint'][3] > .015
    for name in ('point', 'shaped', 'physical'):
        checks['pbr_reference_'+name] = bool(np.allclose(
            rows['pbr_reference_'+name], rows['pbr_'+name]['blend'], atol=1.e-6, rtol=0))
    return checks


def main():
    global OUT
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--shader-root', type=Path)
    ap.add_argument('--out', type=Path, default=OUT)
    args = ap.parse_args()
    OUT = args.out.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    print(gl.renderer, flush=True)
    rows = measure(Runner(args.shader_ref, args.shader_root))
    checks = validate(rows)

    # Negative control is mandatory and runs the same unmodified production
    # fragments read with git show into a scratch snapshot. It cannot merely
    # test whether a source string changed, or pass because compilation failed.
    baseline = measure(Runner('13028c968b4'))
    baseline_checks = validate(baseline)
    required_failures = ('sun', 'sky', 'alpha_point', 'material_point',
        'alpha_physical', 'material_physical', 'pbr_point_relative',
        'pbr_physical_relative', 'alpha_back_facing_blend',
        'material_back_facing_blend', 'glass_sun_glint_alpha')
    checks['baseline_rejects_every_fixed_term'] = all(
        not baseline_checks[name] for name in required_failures)
    failures = [name for name, passed in checks.items() if not passed]
    report = dict(renderer=gl.renderer, shader_ref=args.shader_ref,
        shader_root=str(args.shader_root) if args.shader_root else None,
        tolerance=dict(absolute_rgb=.004, pbr_local_relative=.006, coverage=1.e-6),
        fixture='RGBA8 strand (.8,.82,.85); neutral FX/fog/rim; RGBA32F readback; '
                'real masked G-buffer SRGB8_ALPHA8 (framebuffer conversion off); '
                'sun/sky, near physical light, PBR, front/back and straight alpha',
        measurements=rows, baseline=baseline, checks=checks,
        baseline_checks=baseline_checks, failures=failures)
    (OUT/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    for name, row in rows.items():
        if 'delta_rgb' in row:
            print(name, 'delta RGB', ['%.7f' % x for x in row['delta_rgb']], flush=True)
    print('baseline FAILS:', ', '.join(k for k, v in baseline_checks.items() if not v), flush=True)
    print(('FAIL: '+', '.join(failures)) if failures else
          'PASS: %d production-GL parity and negative-control checks' % len(checks), flush=True)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
