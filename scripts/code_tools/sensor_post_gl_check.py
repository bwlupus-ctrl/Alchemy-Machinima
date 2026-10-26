"""WP-B physical camera GL fixtures. Production GLSL; hidden WGL; no viewer build.

Every subprocess has a watchdog. --reference reads sources without moving HEAD.
Evidence distinguishes production-shader fixtures from viewer integration checks.
"""
import os
os.environ['PYTHONUTF8'] = '1'
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import argparse
import ctypes as C
import hashlib
import json
import re
from pathlib import Path
import subprocess
import sys
import time
import traceback
import numpy as np
import relight_3d_gl as G
import sensor_iso_gl_check as S

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / G.SHADERS
OUT = ROOT / '.tmp/iso_sensor/wpb'
BASELINE = '3efef2be20af4a3098c21e7d3b22cd54ea6c457c'


class Runner:
    def __init__(self, reference=None):
        self.base = S.Runner()
        self.gl = self.base.gl
        self.reference = reference
        self.programs = {}
        self.targets = {}
        self.sources = {}
        self.draw_buffers = self.gl.fn('glDrawBuffers', None, C.c_int, C.POINTER(C.c_uint))
        self.read_buffer = self.gl.fn('glReadBuffer', None, C.c_uint)
        self.sensor_u = None
        print(self.gl.renderer, flush=True)

    def source(self, path, ref=None):
        ref = ref or self.reference
        key = (path, ref)
        if key not in self.sources:
            self.sources[key] = (subprocess.check_output(
                ['git', 'show', ref+':'+G.SHADERS+'/'+path], cwd=ROOT,
                text=True, encoding='utf-8', timeout=15) if ref else
                (SHADERS/path).read_text(encoding='utf-8'))
        return self.sources[key]

    def compile(self, text, defines='', kind=0x8B30):
        stage = '#define VERTEX_SHADER 1\n' if kind == 0x8B31 else '#define FRAGMENT_SHADER 1\n'
        injection = G.PREAMBLE + '\n' + stage + defines + '\n'
        source = text.replace('/*[EXTRA_CODE_HERE]*/', injection, 1) if '/*[EXTRA_CODE_HERE]*/' in text else injection+text
        return self.base.compile('#version 420\n'+source, kind)

    def program(self, path, defines='', modules=(), vertex=None, mutation=None, ref=None):
        key = (path, defines, modules, vertex, mutation, ref)
        if key in self.programs:
            return self.programs[key]
        gl = self.gl
        pr = gl.program()
        for name in modules:
            gl.attach(pr, self.compile(self.source(name, ref), defines))
        source = self.source(path, ref)
        if mutation:
            old, new = mutation
            assert old in source, (path, old)
            source = source.replace(old, new)
        gl.attach(pr, self.compile(source, defines))
        if vertex:
            gl.attach(pr, self.compile(self.source(vertex, ref), defines, 0x8B31))
            if 'SMAA' in path:
                gl.attach(pr, self.compile(self.source('class1/deferred/SMAA.glsl', ref), defines, 0x8B31))
        else:
            gl.attach(pr, self.base.vertex)
        gl.link(pr)
        ok = C.c_int()
        gl.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.plog(pr, len(log), None, log)
            raise AssertionError(path+' LINK '+log.value.decode())
        self.programs[key] = pr
        return pr

    def target(self, name, w, h, count=1, fmt=0x881A):
        key = (name, w, h, count, fmt)
        if key not in self.targets:
            values = []
            for _ in range(count):
                tex = C.c_uint()
                self.gl.gen_tex(1, C.byref(tex))
                self.gl.bind_tex(0x0DE1, tex)
                for p in (0x2801, 0x2800): self.gl.param(0x0DE1, p, 0x2601)
                for p in (0x2802, 0x2803): self.gl.param(0x0DE1, p, 0x812F)
                self.gl.teximage(0x0DE1, 0, fmt, w, h, 0, 0x1908, 0x1406, None)
                values.append(tex)
            self.targets[key] = values
        return self.targets[key]

    def upload(self, name, pixels, fmt=0x881A):
        pixels = np.asarray(pixels, np.float32)
        h, w = pixels.shape[:2]
        if pixels.shape[-1] == 3:
            pixels = np.concatenate([pixels, np.ones((h,w,1), np.float32)], axis=2)
        tex = self.target(name, w, h, fmt=fmt)[0]
        self.gl.bind_tex(0x0DE1, tex)
        self.gl.teximage(0x0DE1, 0, fmt, w, h, 0, 0x1908, 0x1406, pixels.ctypes.data)
        return tex

    def uniform(self, pr, name, value):
        gl = self.gl
        location = gl.loc(pr, name.encode())
        if location < 0: return
        if isinstance(value,np.ndarray) and value.dtype==np.uint32:
            gl.u4uiv(location,1,value.ctypes.data_as(C.POINTER(C.c_uint)))
        elif isinstance(value, (int, np.integer)): gl.ui(location, int(value))
        else:
            values = (value,) if np.isscalar(value) else value
            gl.uf[len(values)](location, *values)

    def sensor(self, pr, u):
        floats = u.view(np.float32)
        for name, index in [('physics',0),('scale',4),('controls',8)]:
            self.gl.u4fv(self.gl.loc(pr, ('sensor.'+name).encode()), 1,
                        floats[index:index+4].ctypes.data_as(C.POINTER(C.c_float)))
        for name, index in [('cameraToWorking',12),('workingToCamera',24),('readCameraBasis',40)]:
            tight = floats[index:index+12].reshape(3,4)[:,:3].copy()
            self.base.matrix3(self.gl.loc(pr, ('sensor.'+name).encode()), 1, 0,
                              tight.ctypes.data_as(C.POINTER(C.c_float)))
        for name, index in [('clock',36),('randomContext',52)]:
            values = u.view(np.uint32)[index:index+4]
            self.gl.u4uiv(self.gl.loc(pr, ('sensor.'+name).encode()), 1,
                         values.ctypes.data_as(C.POINTER(C.c_uint)))

    def draw(self, pr, dst, w, h, textures=(), uniforms=None, read=True, timed=False, matrix=True):
        gl = self.gl
        gl.bind_fbo(0x8D40, self.base.fbo)
        for index in range(3):
            gl.fbo_tex(0x8D40, 0x8CE0+index, 0x0DE1, dst[index] if index < len(dst) else 0, 0)
        buffers = (C.c_uint*len(dst))(*[0x8CE0+i for i in range(len(dst))])
        self.draw_buffers(len(dst), buffers)
        assert gl.fbo_status(0x8D40) == 0x8CD5
        gl.viewport(0,0,w,h)
        gl.use(pr)
        for unit, (name, tex) in enumerate(textures):
            assert tex not in dst, 'texture feedback'
            gl.active(0x84C0+unit)
            gl.bind_tex(0x0DE1, tex)
            self.uniform(pr,name,unit)
        values = dict(screen_res=(w,h),uResolution=(w,h))
        values.update(uniforms or {})
        for name, value in values.items(): self.uniform(pr,name,value)
        if matrix:
            identity = np.eye(4,dtype=np.float32)
            for name in ('inv_proj','modelview_projection_matrix'):
                gl.matrix(gl.loc(pr,name.encode()), 1, 0, identity.ctypes.data_as(C.POINTER(C.c_float)))
        if self.sensor_u is not None and gl.loc(pr,b'sensor.physics') >= 0: self.sensor(pr,self.sensor_u)
        gl.bind_buf(0x8892,self.base.vbo)
        index=gl.attr_loc(pr,b'position')
        gl.enable_attr(index)
        gl.attr_ptr(index,2,0x1406,0,8,None)
        query=C.c_uint()
        if timed:
            gl.gen_queries(1,C.byref(query)); gl.begin_query(0x88BF,query)
        gl.draw(0x0004,0,6)
        if timed:
            gl.end_query(0x88BF)
            ns=C.c_uint64(); gl.query_u64(query,0x8866,C.byref(ns))
            self.base.delete_query(1,C.byref(query))
        assert self.base.error() == 0, 'GL error'
        if not read: return ns.value*1e-6 if timed else None
        result=[]
        for index in range(len(dst)):
            self.read_buffer(0x8CE0+index)
            data=np.empty((h,w,4),np.float32)
            gl.read(0,0,w,h,0x1908,0x1406,data.ctypes.data)
            result.append(data)
        return result


def common_modules():
    return tuple('class1/'+p for p in G.AreaRunner.FRAGMENT_COMMON)


def camera(r, stage, grade=False, ref=None):
    defs = '#define TONEMAP 1\n#define HAS_POST_EFFECTS 1\n#define BLOOM_COMPOSITE 1\n'
    if stage: defs += '#define AL_SENSOR_HDR 1\n#define AL_SENSOR_'+stage+' 1\n'
    modules = common_modules()+('class1/deferred/tonemapUtilF.glsl','class1/deferred/LPMUtil.glsl',
                                 'class1/alchemy/postEffectUtilsF.glsl')
    if grade:
        defs += '#define COLOR_GRADE 1\n'
        modules += ('class1/alchemy/colorGradeUtilF.glsl',)
    return r.program('class1/alchemy/colorCorrectF.glsl',defs,modules,ref=ref)


def smoke(r):
    r.program('class3/deferred/volumetricLightF.glsl','#define AL_SENSOR_HDR 1\n#define SUN_SHADOW 1\n',
        common_modules()+('class1/deferred/shadowUtil.glsl',))
    r.program('class1/deferred/volumetricLightF.glsl','#define AL_SENSOR_HDR 1\n')
    for stage in ('EXPOSURE','OPTICS','CAMERA','GODRAYS'):
        for grade in (False,True): camera(r,stage,grade)
    r.program('class1/alchemy/sensorF.glsl',modules=('class1/alchemy/sensorUtilF.glsl',))
    for path in ('sensorAAEncodeF.glsl','sensorVignetteF.glsl','sensorOverlayF.glsl'):
        r.program('class1/alchemy/'+path)
    for front in (0,1):
        r.program('class1/deferred/postDeferredF.glsl',f'#define AL_SENSOR_HDR 1\n#define FRONT_BLUR {front}\n',common_modules())
        r.program('class1/deferred/postDeferredF.glsl',f'#define AL_SENSOR_HDR 1\n#define FRONT_BLUR {front}\n#define AL_SENSOR_DOF_REDUCE 1\n',common_modules())
        r.program('class1/deferred/dofCombineF.glsl',f'#define AL_SENSOR_HDR 1\n#define FRONT_BLUR {front}\n')
    for path in ('ultimateDiopterGatherF.glsl','ultimateDiopterF.glsl','ultimateKaleidoF.glsl'):
        r.program('class1/deferred/'+path,'#define AL_SENSOR_HDR 1\n#define DIOPTER_TAPS 16\n',common_modules())
    r.program('class1/alchemy/blitWithEffectsF.glsl','#define AL_SENSOR_HDR 1\n',
              common_modules()+('class1/alchemy/postEffectUtilsF.glsl',))
    r.program('class1/deferred/cineOutlineF.glsl','#define AL_SENSOR_HDR 1\n#define AL_SENSOR_OVERLAY 1\n',common_modules())
    for quality in (12,23,28,39):
        r.program('class1/deferred/fxaaF.glsl',f'#define FXAA_GLSL_400 1\n#define FXAA_QUALITY__PRESET {quality}\n',
                  vertex='class1/deferred/postDeferredV.glsl')
    for preset in ('LOW','MEDIUM','HIGH','ULTRA'):
        defs=f'#define SMAA_GLSL_4 1\n#define SMAA_PRESET_{preset} 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n#define AL_SENSOR_HDR 1\n'
        r.program('class1/deferred/SMAANeighborhoodBlendF.glsl',defs,
                  ('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl'),
                  'class1/deferred/SMAANeighborhoodBlendV.glsl')
    return dict(programs=len(r.programs))


def native_host(r):
    import sensor_post_dispatch_check as dispatch
    import sensor_post_recipe_check as recipes
    result=dispatch.run(r,ROOT,OUT/'native_dispatch')
    result['shader_recipes']=recipes.run(ROOT,OUT/'native_recipes')
    return result


def pixels(r, check):
    w,h=128,96
    rng=np.random.default_rng(0x51b)
    data=np.ones((h,w,4),np.float32)
    data[:,:,:3]=rng.uniform(.02,.7,(h,w,3))
    src=r.upload('pixels',data)
    zero=r.upload('zero',np.zeros((h,w,4),np.float32))
    one=r.upload('one',np.ones((h,w,4),np.float32))
    depth=r.upload('depth',np.full((h,w,4),.5,np.float32))
    target=r.target('output',w,h,fmt=0x8814)
    defaults=dict(exposure=2.,tonemap_type=3,tonemap_mix=1.,tonemap_grade=(1.,1.),
        tonemap_params=(36.,0.,0.,0.),bloom_strength=1.)
    tex=[('diffuseRect',src),('depthMap',depth),('bloomMap',zero),('exposureMap',one)]
    exposed=r.draw(camera(r,'EXPOSURE'),target,w,h,tex,defaults)[0]
    check('stage A exposure exactly once',np.max(abs(exposed[:,:,:3]-data[:,:,:3]*2))<.001,
          dict(max_error=float(np.max(abs(exposed[:,:,:3]-data[:,:,:3]*2)))))
    exposed_tex=r.upload('exposed',exposed)
    out=r.draw(camera(r,'CAMERA'),target,w,h,[('diffuseRect',exposed_tex),('exposureMap',one)],defaults)[0]
    # Camera processing must be invariant to both exposure controls and bloom.
    changed=r.draw(camera(r,'CAMERA'),target,w,h,[('diffuseRect',exposed_tex),('exposureMap',zero),('bloomMap',one)],
                   dict(defaults,exposure=4.,bloom_strength=200.))[0]
    check('stage C excludes exposure and bloom',np.array_equal(out,changed))
    twice=r.draw(camera(r,''),target,w,h,[('diffuseRect',exposed_tex),('exposureMap',one)],defaults)[0]
    check('mutation killed: exposure twice',np.max(abs(out-twice))>.01)
    # Perceptual detection must not encode the radiance gathered by FXAA.
    encode=r.program('class1/alchemy/sensorAAEncodeF.glsl')
    linear=r.draw(encode,target,w,h,[('diffuseRect',src)],{'sensor_aa_mode':1})[0]
    check('FXAA auxiliary preserves linear RGB',np.max(abs(linear[:,:,:3]-data[:,:,:3]))<.0005)
    check('FXAA auxiliary uses bounded perceptual luma',bool(np.all(linear[:,:,3]>=0) and np.all(linear[:,:,3]<=1)))
    # Production motion blur on a constant HDR field conserves energy exactly.
    hdr=np.full((h,w,4),16.,np.float32);hdr[:,:,3]=1
    hdrtex=r.upload('hdr',hdr)
    velocity=np.zeros((h,w,4),np.float32);velocity[:,:,0]=.1
    vel=r.upload('velocity',velocity)
    blur=r.program('class1/deferred/motionBlurF.glsl')
    result=r.draw(blur,target,w,h,[('diffuseRect',hdrtex),('velocityMap',vel)],{'motion_blur_strength':32})[0]
    check('motion blur conserves HDR streak energy',np.max(abs(result[:,:,:3]-16.))<1e-5)
    # A noise-free constant HDR field checks both the no-clamp path and CoC.
    cof=hdr.copy();cof[:,:,3]=0.
    coftex=r.upload('cof',cof)
    dof=r.program('class1/deferred/postDeferredF.glsl','#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 1\n',common_modules())
    result=r.draw(dof,target,w,h,[('diffuseRect',coftex)],{'max_cof':4.,'res_scale':1.})[0]
    check('DOF linear HDR gather preserves constant radiance',np.max(abs(result[:,:,:3]-16.))<1e-4)
    weighted=dof_weight_probe(r,hdrtex,target,w,h)
    check('DOF weights use bounded encoded luma with linear samples',
          np.max(abs(weighted[:,:,0]-(.25+16./17.)))<1e-5 and np.max(abs(weighted[:,:,1:]-16.))<1e-5)
    # Both AA implementations must leave a constant HDR image bit-identical.
    fxaa=r.program('class1/deferred/fxaaF.glsl','#define FXAA_GLSL_400 1\n#define FXAA_QUALITY__PRESET 28\n',
                   vertex='class1/deferred/postDeferredV.glsl')
    encoded=r.draw(encode,target,w,h,[('diffuseRect',hdrtex)],{'sensor_aa_mode':1})[0]
    encoded_tex=r.upload('hdr-rgbl',encoded)
    result=r.draw(fxaa,target,w,h,[('diffuseMap',encoded_tex),('depthMap',depth)],
                  {'tc_scale':(1.,1.),'rcp_screen_res':(1/w,1/h)})[0]
    check('FXAA HDR no-edge identity',np.max(abs(result[:,:,:3]-16.))<1e-5)
    defs='#define SMAA_GLSL_4 1\n#define SMAA_PRESET_HIGH 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n#define AL_SENSOR_HDR 1\n'
    modules=('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl')
    smaa=r.program('class1/deferred/SMAANeighborhoodBlendF.glsl',defs,modules,
                   'class1/deferred/SMAANeighborhoodBlendV.glsl')
    result=r.draw(smaa,target,w,h,[('diffuseRect',hdrtex),('blendTex',zero)],{'SMAA_RT_METRICS':(1/w,1/h,w,h)})[0]
    check('SMAA HDR no-edge identity',np.array_equal(result,hdr))
    # Nonzero weights distinguish linear blending from the legacy sRGB curve.
    edge=data.copy();edge[:,:w//2,:3]=.1;edge[:,w//2:,:3]=8.
    edge_tex=r.upload('edge',edge)
    weights=np.zeros((h,w,4),np.float32)
    weights[:,:,2:]=.5
    weighttex=r.upload('weights',weights)
    tex=[('diffuseRect',edge_tex),('blendTex',weighttex)]
    blend=r.draw(smaa,target,w,h,tex,{'SMAA_RT_METRICS':(1/w,1/h,w,h)})[0]
    legacy=r.program('class1/deferred/SMAANeighborhoodBlendF.glsl',defs.replace('#define AL_SENSOR_HDR 1\n',''),modules,
                     'class1/deferred/SMAANeighborhoodBlendV.glsl')
    mutated=r.draw(legacy,target,w,h,tex,{'SMAA_RT_METRICS':(1/w,1/h,w,h)})[0]
    expected=np.array([2.075,6.025])[None,:,None]
    check('SMAA blends original linear HDR at edges',np.max(abs(blend[:,w//2-1:w//2+1,:3]-expected))<.001,
          dict(observed=blend[h//2,w//2-1:w//2+1,0].tolist(),expected=expected.ravel().tolist()))
    check('mutation killed: SMAA sRGB conversions',np.max(abs(blend-mutated))>.05,
          dict(max_difference=float(np.max(abs(blend-mutated)))))
    # Legacy source variants must still render exactly the baseline full frame.
    for grade in (False,True):
        a=camera(r,'',grade);b=camera(r,'',grade,BASELINE)
        uniforms=dict(defaults,uWhiteBalanceGain=(1.,1.,1.),uLift=(0.,0.,0.),uGain=(1.,1.,1.),uInvGammaCC=(1.,1.,1.),
            uBWPScale=1.,uBCScale=1.,uSaturation=1.,uCurveInvRange=(1.,1.,1.))
        tex=[('diffuseRect',src),('depthMap',depth),('bloomMap',zero),('exposureMap',one)]
        left=r.draw(a,target,w,h,tex,uniforms)[0];right=r.draw(b,target,w,h,tex,uniforms)[0]
        check('legacy colorCorrect full-frame byte identity grade='+str(grade),np.array_equal(left.view(np.uint32),right.view(np.uint32)))


def dof_weight_probe(r,source,target,w,h):
    path='class1/deferred/postDeferredF.glsl'
    text=r.source(path);tail=text[text.index('void main()'):]
    probe=tail.replace('void main()','void productionDofMain()',1)+'''
void main()
{
    vec4 value=vec4(0.0); float weight=0.0;
    dofSampleNear(value,weight,0.0,vary_fragcoord);
    frag_color=vec4(weight,value.rgb/weight);
}
'''
    program=r.program(path,'#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 1\n',common_modules(),mutation=(tail,probe))
    return r.draw(program,target,w,h,[('diffuseRect',source)],{'max_cof':4.,'res_scale':1.})[0]


def sensor_order(r, check):
    """Noise placement, processing and overlay predicates on production passes.
    Mutations are deliberately different pass orders, not artificial failures.
    """
    model=S.Model(OUT/'model')
    w,h=256,192
    noise,params=model.get(0,25600,frame=9,seed=1234,stream=0x4d41494e)
    r.sensor_u=noise
    sensor=r.program('class1/alchemy/sensorF.glsl',modules=('class1/alchemy/sensorUtilF.glsl',))
    target=r.target('sensor-order',w,h,fmt=0x8814)
    plain=np.full((h,w,4),.18,np.float32);plain[:,:,3]=0.
    plaintex=r.upload('sensor-plain',plain)
    raw=r.draw(sensor,target,w,h,[('diffuseRect',plaintex)])[0]
    copy=camera(r,'EXPOSURE')
    clean=r.draw(copy,target,w,h,[('diffuseRect',plaintex)],{'tonemap_type':8,'exposure':1.})[0]
    # Dispatch the same actual production sensor program using the C++ gate.
    context_rows=[]
    for mask,name in ((1,'probe'),(7,'cube'),(11,'Prism'),(19,'Prism preview'),(35,'impostor'),(67,'HUD'),(131,'no post')):
        allowed=bool(r.native_lib.gate(mask))
        presented=raw if allowed else clean
        context_rows.append(dict(context=name,unchanged=bool(np.array_equal(presented,clean))))
    check('auxiliary and no-post contexts stay un-noised',all(row['unchanged'] for row in context_rows),context_rows)
    cube=raw if r.native_lib.gate_leak(7) else clean
    check('mutation killed: sensor on cube context',not np.array_equal(cube,clean) and not r.native_lib.gate(7))
    rawtex=r.upload('sensor-noisy',raw)
    dof=r.program('class1/deferred/postDeferredF.glsl','#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 1\n',common_modules())
    blurred=r.draw(dof,target,w,h,[('diffuseRect',plaintex)],{'max_cof':4.,'res_scale':1.})[0]
    blurtex=r.upload('sensor-blurred-plain',blurred)
    after=r.draw(sensor,target,w,h,[('diffuseRect',blurtex)])[0]
    variance=lambda x:float(np.var(x[12:-12,12:-12,:3]))
    check('sensor noise after DOF equals in-focus statistics',np.array_equal(raw,after),
          dict(in_focus=variance(raw),after_dof=variance(after)))
    mutated=r.draw(dof,target,w,h,[('diffuseRect',rawtex)],{'max_cof':4.,'res_scale':1.})[0]
    check('mutation killed: noise before DOF',variance(mutated)<variance(raw)*.65,
          dict(before_dof=variance(mutated),correct=variance(raw)))
    # SMAA explicitly blends a dense fixture. Applying it after collection
    # measurably smooths noise. Correct order collects on its linear result.
    weights=np.zeros((h,w,4),np.float32);weights[:,:,2:]=.5
    weighttex=r.upload('noise-weights',weights)
    defs='#define SMAA_GLSL_4 1\n#define SMAA_PRESET_HIGH 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n#define AL_SENSOR_HDR 1\n'
    smaa=r.program('class1/deferred/SMAANeighborhoodBlendF.glsl',defs,
        ('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl'),'class1/deferred/SMAANeighborhoodBlendV.glsl')
    smoothed=r.draw(smaa,target,w,h,[('diffuseRect',rawtex),('blendTex',weighttex)],
                    {'SMAA_RT_METRICS':(1/w,1/h,w,h)})[0]
    check('mutation killed: noise before AA',variance(smoothed)<variance(raw)*.85,
          dict(wrong_order=variance(smoothed),correct=variance(raw)))
    # A coloured opaque guide is composited in display space and stays exact,
    # irrespective of the underlying sensor stream.
    addition=np.zeros((h,w,4),np.float32);transmission=np.ones_like(addition)
    addition[:,w//3:2*w//3,:3]=(.25,.5,.75)
    transmission[:,w//3:2*w//3,:3]=0.
    addtex=r.upload('guide-add',addition);trtex=r.upload('guide-transmission',transmission)
    overlay=r.program('class1/alchemy/sensorOverlayF.glsl')
    output=r.draw(overlay,target,w,h,[('diffuseRect',rawtex),('diffuseMap',addtex),('emissiveRect',trtex)],
                  {'depth_warp_params':(0.,0.,w/h,0.),'sensor_overlay_mode':0})[0]
    guide=output[2:-2,w//3+2:2*w//3-2,:3]
    check('guides remain exact and un-noised after sensor',np.max(abs(guide-np.array([.25,.5,.75])))<1e-6)
    outputtex=r.upload('wrong-noised-guide',output)
    wrong=r.draw(sensor,target,w,h,[('diffuseRect',outputtex)])[0]
    check('mutation killed: guides noised',np.var(wrong[2:-2,w//3+2:2*w//3-2,0])>1e-6)
    # S-Log3 processing bypasses a hostile grade and manual exposure setting.
    black=np.zeros_like(plain);black[:,:,3]=1.
    blacktex=r.upload('sensor-black',black)
    a=r.draw(sensor,target,w,h,[('diffuseRect',blacktex)])[0]
    signed=r.upload('signed-black',a,fmt=0x8814)
    values=[]
    for grade in (False,True):
        cp=camera(r,'CAMERA',grade)
        result=r.draw(cp,target,w,h,[('diffuseRect',signed)],dict(
            tonemap_type=8,tonemap_params=(0.,0.,0.,0.),exposure=4.,uGain=(7.,2.,3.),uLift=(1.,1.,1.),
            uInvGammaCC=(.2,.3,.4)))[0]
        values.append(result)
    check('S-Log3 sensor black codes and grade bypass',np.array_equal(*values) and np.isfinite(values[0]).all() and
          .092 < float(values[0][:,:,:3].mean()) < .3,
          dict(mean_code=float(values[0][:,:,:3].mean()),signed_mean=float(a[:,:,:3].mean())))
    # Adversarial scene values go through actual exposure, sensor and camera.
    values=np.array([np.nan,-np.inf,np.inf,-1e30,-1.,0.,1e30],np.float32)
    bad=np.ones((h,w,4),np.float32);bad[:,:,:3]=np.resize(values,(h,w,3))
    badtex=r.upload('adversarial',bad,fmt=0x8814)
    exposed=r.draw(camera(r,'EXPOSURE'),target,w,h,[('diffuseRect',badtex)],{'tonemap_type':8,'exposure':4.})[0]
    exposedtex=r.upload('adversarial-exposed',exposed)
    result=r.draw(sensor,target,w,h,[('diffuseRect',exposedtex)])[0]
    check('actual exposure/sensor NaN negative extreme HDR finite',np.isfinite(result).all())
    # A repeated packed capture clock gives exactly the same final image.
    repeated=r.draw(sensor,target,w,h,[('diffuseRect',plaintex)])[0]
    check('capture-relative fixed-seed determinism',np.array_equal(repeated,raw))
    r.sensor_u=None


def optics_alignment(r,check):
    w,h=160,120
    image=np.zeros((h,w,4),np.float32);image[:,:,3]=1
    image[30:65,95:130,1]=3.
    src=r.upload('alignment-scene',image)
    bloom=np.zeros_like(image);bloom[30:65,95:130,0]=2.
    bloomtex=r.upload('alignment-bloom',bloom)
    one=r.upload('alignment-one',np.ones_like(image))
    output=r.target('alignment-out',w,h,fmt=0x8814)
    inputs=[('diffuseRect',src),('diffuseMap',src),('depthMap',one),('exposureMap',one),('bloomMap',bloomtex)]
    optical=r.draw(camera(r,'OPTICS'),output,w,h,inputs,
                   {'bloom_strength':1.,'exposure':1.,'tonemap_type':8})[0]
    opticaltex=r.upload('alignment-optical',optical)
    fish=r.program('class1/deferred/cineFisheyeF.glsl')
    params={'fisheye_params':(1.2,.3,1.5,.01),'fisheye_params2':(0.,0.,1.,0.)}
    correct=r.draw(fish,output,w,h,[('diffuseMap',opticaltex)],params)[0]
    warped_scene=r.draw(fish,output,w,h,[('diffuseMap',src)],params)[0]
    wrongtex=r.upload('bloom-after-warp',warped_scene)
    inputs[0]=('diffuseRect',wrongtex)
    wrong=r.draw(camera(r,'OPTICS'),output,w,h,inputs,
                 {'bloom_strength':1.,'exposure':1.,'tonemap_type':8})[0]
    check('bloom follows fisheye with the scene',np.max(abs(correct[:,:,0]*1.5-correct[:,:,1]))<.002)
    check('mutation killed: bloom after warps',np.max(abs(wrong[:,:,0]*1.5-wrong[:,:,1]))>1.)
    # Overlay samples the EXACT production diopter attachment-2 map. Obtain it
    # from a real kaleidoscope MRT, then apply the common fisheye source map.
    kal=r.program('class1/deferred/ultimateKaleidoF.glsl','#define AL_SENSOR_HDR 1\n',common_modules())
    mrt=r.target('alignment-kal',w,h,3)
    kalparams=dict(kal_pattern=(0.,6.,1.,0.),kal_shape=(.2,0.,0.,0.),kal_center=(.5,.5,.5,.5),
        kal_source=(0.,1.,0.,0.),kal_look=(0.,1.,0.,0.),kal_protect=(0.,0.,0.,0.))
    kalresult=r.draw(kal,mrt,w,h,[('diffuseRect',src),('depthMap',one)],kalparams)
    combined=r.draw(kal,mrt,w,h,[('diffuseRect',opticaltex),('depthMap',one)],kalparams)
    kaloptical=r.upload('alignment-kal-optical',combined[0])
    combined_warp=r.draw(fish,output,w,h,[('diffuseMap',kaloptical)],params)[0]
    check('bloom follows kaleidoscope and fisheye before sensor',
          np.max(abs(combined_warp[:,:,0]*1.5-combined_warp[:,:,1]))<.003)
    # Restore the scene-only MRT and its geometry map for the guide comparison.
    kalresult=r.draw(kal,mrt,w,h,[('diffuseRect',src),('depthMap',one)],kalparams)
    target=r.target('alignment-kal-fish',w,h,fmt=0x8814)
    warped=r.draw(fish,target,w,h,[('diffuseMap',mrt[0])],params)[0]
    add=image.copy();add[:,:,:3]=image[:,:,:3]/3.
    addtex=r.upload('alignment-guide',add)
    trtex=r.upload('alignment-transmission',np.zeros_like(image))
    overlay=r.program('class1/alchemy/sensorOverlayF.glsl')
    unwarped=r.upload('alignment-base',np.zeros_like(image))
    guides=r.draw(overlay,target,w,h,[('diffuseRect',unwarped),('diffuseMap',addtex),
         ('emissiveRect',trtex),('diopterWarpMap',mrt[2])],dict(params,
         depth_warp_params=(1.,1.,w/h,0.),sensor_overlay_mode=0))[0]
    yy,xx=np.mgrid[0:h,0:w];inside=((xx+.5)/w*2-1)**2*(w/h)**2+((yy+.5)/h*2-1)**2 < 1.4**2
    guide_mask=(guides[:,:,1]>.5)&inside
    image_mask=(warped[:,:,1]>1.5)&inside
    def distance(a,b):
        for count in range(9):
            if not np.any(b&~a): return count
            a=np.logical_or.reduce([np.roll(np.roll(a,x,axis=1),y,axis=0)
                                    for x in (-1,0,1) for y in (-1,0,1)])
        return 9
    maximum=max(distance(guide_mask.copy(),image_mask),distance(image_mask.copy(),guide_mask))
    check('guides align with kaleidoscope attachment-2 and fisheye',maximum<=1.,
          dict(maximum_edge_displacement_pixels=maximum,
               max_raster_difference=float(np.max(abs(guides[:,:,1][inside]-warped[:,:,1][inside]/3.)))))


def source_contracts(check):
    pipe=(ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    start=pipe.index('// Stage A: expose exactly once')
    end=pipe.index('        renderFinalizeLegacy(',start)
    chain=pipe[start:end]
    calls=['SensorStage::Exposure','renderVolumetric(','renderMotionBlurComposite(',
        'renderDoF(','SensorStage::Optics','EVisualEffect::RlvSphere','renderUltimateKaleidoscope(',
        'renderCineFisheye(','sensorVignette(','applyFXAA(','renderSensor(',
        'SensorStage::Camera','applyCAS(','compositeSensorOverlay(']
    # Roto's un-noised reference is processed before the warps, but it is NOT
    # the camera image. Search stage C starting after collection.
    pos=[]
    for call in calls:
        offset=chain.index('renderSensor(') if call=='SensorStage::Camera' else 0
        pos.append(chain.index(call,offset))
    check('production main stage-order contract',pos==sorted(pos),list(zip(calls,pos)))
    check('separate physical-order and noise isolation flags',
          'mSensorNoise = mSensorPhysicalOrder && sensor_noise()' in pipe and
          'mSensorNoise ? amount() : 0.f' in pipe)
    recipe=pipe.rfind('bool LLPipeline::allocateScreenBufferInternal(',0,pipe.index('U32 post_color_fmt'))
    current=pipe[recipe:]
    current=current[:current.index('\n}')+2]
    old=subprocess.check_output(['git','show',BASELINE+':indra/newview/pipeline.cpp'],cwd=ROOT,text=True,encoding='utf-8',timeout=15)
    recipe=old.rfind('bool LLPipeline::allocateScreenBufferInternal(',0,old.index('U32 post_color_fmt'))
    old=old[recipe:];old=old[:old.index('\n}')+2]
    check('existing target allocation recipe unchanged',current==old)
    changed=current.replace('GL_RGB10_A2','GL_RGBA16F')
    check('mutation killed: existing target format changed',changed!=old)
    baseline=subprocess.check_output(['git','show',BASELINE+':indra/newview/pipeline.cpp'],cwd=ROOT,
        text=True,encoding='utf-8',timeout=15)
    def legacy_body(source):
        begin=source.index('// Handles tonemap, colorgrading')
        if 'void LLPipeline::renderFinalizeLegacy(' in source:
            finish=source.index('void LLPipeline::renderFinalizePresent(',begin)
            body=source[begin:finish];body=body[:body.rfind('}')]
        else:
            finish=source.index('    if (RenderBufferVisualization > -1)',begin)
            body=source[begin:finish]
        body=body.replace('LLRenderTarget* sourceBuffer =','sourceBuffer =').replace('LLRenderTarget* targetBuffer =','targetBuffer =')
        body=body.replace('bool diopter_warped = false;','').replace('const bool fisheye_warped =','fisheye_warped =')
        return re.sub(r'\s+','',body)
    check('legacy finalize branch retains the original statements and order',legacy_body(pipe)==legacy_body(baseline))
    settings=(ROOT/'indra/newview/app_settings/settings_alchemy.xml').read_text(encoding='utf-8')
    import xml.etree.ElementTree as ET
    root=ET.fromstring(settings).find('map');items=list(root);values={}
    for key,value in zip(items[::2],items[1::2]):
        if key.text.startswith('CineSensor'):
            children=list(value);values[key.text]={k.text:v.text for k,v in zip(children[::2],children[1::2])}
    check('internal sensor settings persist with default off',values['CineSensorEnabled']['Value']=='false' and
          all(v['Persist']=='1' for v in values.values()),dict(settings=len(values)))


def pre_wpb_predicates(reference):
    """These four behavior predicates must reject the pre-WP-B production
    programs individually, without depending on absent new shader files.
    """
    OUT.mkdir(parents=True,exist_ok=True)
    r=Runner(reference);w,h=64,48
    image=np.full((h,w,4),16.,np.float32);image[:,:,3]=1.
    src=r.upload('pre-input',image);one=r.upload('pre-one',np.ones_like(image))
    dst=r.target('pre-output',w,h,fmt=0x8814);rows=[]
    def reject(name,passed):
        rows.append(dict(predicate=name,reference_passed=bool(passed)))
        print(name+(' REJECTED' if not passed else ' UNEXPECTED PASS'),flush=True)
    exposed=r.draw(camera(r,'EXPOSURE'),dst,w,h,[('diffuseRect',src),('exposureMap',one)],
        dict(exposure=2.,tonemap_type=3,tonemap_mix=1.,tonemap_grade=(1.,1.),tonemap_params=(36.,0.,0.,0.)))[0]
    reject('stage A linear exposure',np.max(abs(exposed[:,:,:3]-32.))<1e-4)
    weighted=dof_weight_probe(r,src,dst,w,h)
    reject('DOF bounded encoded-luma weights',np.max(abs(weighted[:,:,0]-(.25+16./17.)))<1e-5)
    kal=r.program('class1/deferred/ultimateKaleidoF.glsl','#define AL_SENSOR_HDR 1\n',common_modules())
    mrt=r.target('pre-mrt',w,h,3)
    output=r.draw(kal,mrt,w,h,[('diffuseRect',src),('depthMap',one)],dict(kal_pattern=(0.,6.,1.,0.),
        kal_center=(.5,.5,.5,.5),kal_source=(0.,1.,0.,0.),kal_look=(0.,1.,0.,0.)))[0]
    reject('kaleidoscope linear HDR output',np.max(abs(output[:,:,:3]-16.))<1e-4)
    dio=r.program('class1/deferred/ultimateDiopterF.glsl','#define AL_SENSOR_HDR 1\n',common_modules())
    output=r.draw(dio,dst,w,h,[('diffuseRect',src),('emissiveRect',src),('diffuseMap',src),('depthMap',one)],
        dict(diopter_shape=(.5,.5,.4,1.),diopter_comp2=(0.,0.,1.,0.)))[0]
    reject('diopter linear HDR output',np.max(abs(output[:,:,:3]-16.))<1e-4)
    defs='#define SMAA_GLSL_4 1\n#define SMAA_PRESET_HIGH 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n#define AL_SENSOR_HDR 1\n'
    smaa=r.program('class1/deferred/SMAANeighborhoodBlendF.glsl',defs,
        ('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl'),'class1/deferred/SMAANeighborhoodBlendV.glsl')
    image[:,:w//2,:3]=.1;image[:,w//2:,:3]=8.;src=r.upload('pre-edge',image)
    weights=np.zeros_like(image);weights[:,:,2:]=.5;weights=r.upload('pre-weights',weights)
    output=r.draw(smaa,dst,w,h,[('diffuseRect',src),('blendTex',weights)],{'SMAA_RT_METRICS':(1/w,1/h,w,h)})[0]
    reject('SMAA linear HDR edge blend',np.max(abs(output[:,w//2-1:w//2+1,:3]-np.array([2.075,6.025])[None,:,None]))<.001)
    (OUT/'pre_wpb_predicates.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    return int(any(row['reference_passed'] for row in rows))


def run(args):
    OUT.mkdir(parents=True,exist_ok=True)
    r=Runner(args.reference)
    checks=[]
    def check(name,passed,details=None):
        checks.append(dict(name=name,passed=bool(passed),details=details))
        print(('PASS ' if passed else 'FAIL ')+name,flush=True)
    error=None
    try:
        check('production shader variants compile/link',True,smoke(r))
        check('production context gate and real-GL target lifecycle',True,native_host(r))
        pixels(r,check)
        sensor_order(r,check)
        optics_alignment(r,check)
        source_contracts(check)
        import sensor_post_fix_gl_check as review
        review.tests(r,check)
        import sensor_post_fix2_gl_check as review2
        review2.tests(r,check)
    except Exception:
        error=traceback.format_exc(); print(error,flush=True)
        check('production shader variants compile/link',False,error)
    evidence=dict(checks=checks,error=error,renderer=r.gl.renderer,
        commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True,timeout=15).strip(),
        cpp_sources={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('indra/newview/pipeline.cpp','indra/newview/pipeline.h','indra/newview/llviewershadermgr.cpp',
             'indra/newview/llviewershadermgr.h','indra/newview/rlveffects.cpp','indra/newview/app_settings/settings_alchemy.xml')},
        sources={str(k):hashlib.sha256(v.encode()).hexdigest() for k,v in r.sources.items()})
    (OUT/'results.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
    return int(any(not c['passed'] for c in checks))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker',action='store_true')
    parser.add_argument('--timeout',type=int,default=600)
    parser.add_argument('--reference')
    parser.add_argument('--pre-wpb-ref',help='Expect the four new HDR predicates to reject this read-only revision')
    args=parser.parse_args()
    if args.worker: return pre_wpb_predicates(args.pre_wpb_ref) if args.pre_wpb_ref else run(args)
    return subprocess.run([sys.executable,__file__,'--worker',*sys.argv[1:]],cwd=ROOT,
                          env=dict(os.environ,PYTHONUTF8='1'),timeout=args.timeout).returncode


if __name__=='__main__': raise SystemExit(main())
