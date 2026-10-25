"""WP-B paired GPU timestamps at 4K, production shaders, no viewer build.

Matched ON/OFF fixture: 32-tap motion blur, DOF CoC/gather/combine, SMAA High
with production area/search textures, 24-tap diopter, fisheye, CAS .4 and
presentation. DOF uses the production half-resolution gather contract.
This is a shader-chain benchmark; it does not claim a running-viewer result.
"""
import os
os.environ['PYTHONUTF8']='1'
os.environ['PYTHONDONTWRITEBYTECODE']='1'
import argparse
import ctypes as C
import json
import re
import subprocess
import sys
import time
import numpy as np
import sensor_post_gl_check as P
import sensor_iso_gl_check as S


def lookup(r,name,width,height,channels):
    text=(P.ROOT/'indra/newview'/name).read_text(encoding='utf-8')
    body=text[text.index('Bytes[]'):]
    values=np.array([int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{2})',body)],np.float32)/255.
    assert values.size==width*height*channels,(name,values.size)
    pixels=np.zeros((height,width,4),np.float32)
    pixels[:,:,:channels]=values.reshape(height,width,channels)
    return r.upload(name,pixels)


def gpu():
    try:
        return subprocess.run(['nvidia-smi','--query-gpu=utilization.gpu,clocks.sm,power.draw',
            '--format=csv,noheader'],capture_output=True,text=True,timeout=15).stdout.strip()
    except FileNotFoundError:return 'unavailable'


class Chain:
    def __init__(self,r,model,enabled,w=3840,h=2160):
        self.r,self.on,self.w,self.h=r,enabled,w,h
        self.u,_=model.get(0,25600,frame=0,seed=1234,stream=0x4d41494e)
        self.hdr=[r.target(('on' if enabled else 'off')+str(i),w,h,fmt=0x881A if enabled else 0x8059) for i in range(2)]
        self.display=[r.target('on-display'+str(i),w,h,fmt=0x8059) for i in range(2)] if enabled else self.hdr
        self.dst=self.hdr
        self.cof=r.target('cof',w,h)
        self.edge=r.target('edges',w,h)
        self.weights=r.target('weights',w,h)
        self.dio=r.target('diopter',w,h,3)
        self.encode=r.target('encoded',w,h)
        x=np.arange(w,dtype=np.float32)[None,:]/w
        y=np.arange(h,dtype=np.float32)[:,None]/h
        image=np.empty((h,w,4),np.float32)
        image[:,:,:3]=(.02+2*x[:,:,None])*(.8+.2*np.sin(y[:,:,None]*70))
        image[:,:,3]=1.
        self.scene=r.upload('cost-scene',image)
        image[:,:,:3]=.5;self.depth=r.upload('cost-depth',image)
        image[:,:,:3]=0.;image[:,:,0]=.012;self.velocity=r.upload('cost-velocity',image)
        image[:,:,:3]=.02;self.bloom=r.upload('cost-bloom',image)
        image[:,:,:3]=1.;self.one=r.upload('cost-one',image)
        self.area=lookup(r,'SMAAAreaTex.h',160,560,2)
        self.search=lookup(r,'SMAASearchTex.h',64,16,1)
        self.defaults=dict(exposure=1.,tonemap_type=3,tonemap_mix=1.,tonemap_grade=(1.,1.),
             tonemap_params=(36.,0.,0.,0.),bloom_strength=.325)
        self.programs={}
        for stage in (('EXPOSURE','OPTICS','CAMERA') if enabled else ('',)):
            self.programs['camera'+stage]=P.camera(r,stage)
        self.programs['blur']=r.program('class1/deferred/motionBlurF.glsl')
        self.programs['cof']=r.program('class1/deferred/cofF.glsl',modules=P.common_modules())
        self.programs['dof']=r.program('class1/deferred/postDeferredF.glsl',
            '#define FRONT_BLUR 1\n'+('#define AL_SENSOR_HDR 1\n' if enabled else ''),P.common_modules())
        self.programs['combine']=r.program('class1/deferred/dofCombineF.glsl')
        self.programs['dio']=r.program('class1/deferred/ultimateDiopterGatherF.glsl',
            '#define DIOPTER_TAPS 24\n'+('#define AL_SENSOR_HDR 1\n' if enabled else ''),P.common_modules())
        self.programs['dio_comp']=r.program('class1/deferred/ultimateDiopterF.glsl',
            '#define AL_SENSOR_HDR 1\n' if enabled else '',P.common_modules())
        self.programs['fish']=r.program('class1/deferred/cineFisheyeF.glsl')
        defs='#define SMAA_GLSL_4 1\n#define SMAA_PRESET_HIGH 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n'
        mods=('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl')
        for key,part in [('smaa_edge','EdgeDetect'),('smaa_weights','BlendWeights'),('smaa_blend','NeighborhoodBlend')]:
            self.programs[key]=r.program('class1/deferred/SMAA'+part+'F.glsl',
                defs+('#define AL_SENSOR_HDR 1\n' if enabled and key=='smaa_blend' else ''),mods,
                'class1/deferred/SMAA'+part+'V.glsl')
        self.programs['encode']=r.program('class1/alchemy/sensorAAEncodeF.glsl')
        self.programs['sensor']=r.program('class1/alchemy/sensorF.glsl',modules=('class1/alchemy/sensorUtilF.glsl',))
        self.programs['vignette']=r.program('class1/alchemy/sensorVignetteF.glsl')
        self.programs['cas']=r.program('class1/deferred/CASF.glsl',modules=('class1/environment/srgbF.glsl',))
        self.programs['present']=r.program('class1/alchemy/blitWithEffectsF.glsl',
            '#define AL_SENSOR_HDR 1\n' if enabled else '',P.common_modules()+('class1/alchemy/postEffectUtilsF.glsl',))
        self.out=r.target('cost-present',w,h,fmt=0x8059)
        self.times={}

    def execute(self,frame):
        r,w,h=self.r,self.w,self.h
        self.dst=self.hdr
        S.refresh_random_context(self.u)
        self.u[S.CLOCK:S.CLOCK+8].view(np.uint32)[:]=(frame,0)
        S.refresh_random_context(self.u)
        r.sensor_u=self.u
        self.times={}
        def draw(key,dst,textures,uniforms=None,dims=None):
            ww,hh=dims or (w,h)
            self.times[key]=self.times.get(key,0.)+r.draw(self.programs[key],dst,ww,hh,textures,uniforms,
                read=False,timed=True)
        i=0
        def apply(key,extra=(),uniforms=None):
            nonlocal i
            draw(key,self.dst[1-i],[('diffuseRect',self.dst[i][0]),*extra],uniforms);i=1-i
        tex=[('diffuseRect',self.scene),('diffuseMap',self.scene),('depthMap',self.depth),
             ('bloomMap',self.bloom),('exposureMap',self.one)]
        draw('cameraEXPOSURE' if self.on else 'camera',self.dst[i],tex,self.defaults)
        def dof():
            nonlocal i
            # A physically authored constant CoC fixture: max 4 px, half-res.
            draw('cof',self.cof,[('diffuseRect',self.dst[i][0]),('depthMap',self.depth)],
                dict(focal_distance=-8.,blur_constant=.002,tan_pixel_angle=.0002,magnification=1.,max_cof=4.,res_scale=.5))
            r.gl.color_mask(1,1,1,0)
            draw('dof',self.dst[i],[('diffuseRect',self.cof[0])],
                dict(screen_res=(w,h),max_cof=4.,res_scale=.5),(w//2,h//2))
            r.gl.color_mask(1,1,1,1)
            draw('combine',self.dst[1-i],[('diffuseRect',self.dst[i][0]),('lightMap',self.cof[0])],
                dict(max_cof=4.,res_scale=.5,dof_width=(w//2-1)/w,dof_height=(h//2-1)/h));i=1-i
        def aa():
            nonlocal i
            detection=self.dst[i][0]
            if self.on:
                draw('encode',self.encode,[('diffuseRect',detection)],{'sensor_aa_mode':2});detection=self.encode[0]
            metrics={'SMAA_RT_METRICS':(1/w,1/h,w,h)}
            draw('smaa_edge',self.edge,[('diffuseRect',detection)],metrics)
            draw('smaa_weights',self.weights,[('edgesTex',self.edge[0]),('areaTex',self.area),('searchTex',self.search)],metrics)
            apply('smaa_blend',[('blendTex',self.weights[0])],metrics)
        def optics():
            nonlocal i
            shape=dict(diopter_shape=(.5,.5,.6,1.),diopter_shape2=(0.,.06,0.,0.),
               diopter_shape3=(0.,0.,0.,0.),diopter_focus=(8.,1.5,.4,6.),
               diopter_focus2=(1.,1.,1.,24.),diopter_focus3=(24.,0.,.5,1.),
               diopter_aperture=(0.,6.,0.,1.),diopter_aperture2=(.45,1.,0.,0.),
               exposure=1.,tonemap_type=3)
            draw('dio',self.dio,[('diffuseRect',self.dst[i][0]),('depthMap',self.depth),('exposureMap',self.one)],shape)
            draw('dio_comp',self.dst[1-i],[('diffuseRect',self.dio[0]),('emissiveRect',self.dio[1]),
               ('diffuseMap',self.dst[i][0]),('depthMap',self.depth),('exposureMap',self.one)],
               dict(shape,diopter_comp=(0.,0.,0.,.05),diopter_comp2=(0.,0.,1.,0.)));i=1-i
            draw('fish',self.dst[1-i],[('diffuseMap',self.dst[i][0])],
                {'fisheye_params':(1.2,.3,.92,.18),'fisheye_params2':(0.,0.,1.,0.)});i=1-i
        if self.on:
            apply('blur',[('velocityMap',self.velocity)],{'motion_blur_strength':32})
            dof()
            apply('cameraOPTICS',[('diffuseMap',self.scene),('bloomMap',self.bloom),('exposureMap',self.one),('depthMap',self.depth)],self.defaults)
            optics();apply('vignette',uniforms=dict(uVignetteAmount=.2,uVignetteRadius=1.,uVignetteSoft=.5,
                uVignetteFeather=1.,uVignetteAspect=(1.,1.)))
            aa();apply('sensor')
            draw('cameraCAMERA',self.display[0],[('diffuseRect',self.dst[i][0])],self.defaults)
            self.dst=self.display;i=0
        else:aa()
        c0=np.array([1.,1.,0.,0.],np.float32).view(np.uint32)
        c1=np.array([-1./(8.-3.*.4),0.,0.,0.],np.float32).view(np.uint32)
        apply('cas',uniforms=dict(out_screen_res=(w,h),cas_param_0=c0,cas_param_1=c1))
        if not self.on:
            apply('blur',[('velocityMap',self.velocity)],{'motion_blur_strength':32});dof();optics()
        draw('present',self.out,[('diffuseRect',self.dst[i][0]),('depthMap',self.depth),('diopterWarpMap',self.dio[2])],
             dict(uVignetteAmount=.2,uVignetteRadius=1.,uVignetteSoft=.5,uVignetteFeather=1.,uVignetteAspect=(1.,1.),
                  depth_warp_params=(1.,1.,w/h,0.),fisheye_params=(1.2,.3,.92,.18),fisheye_params2=(0.,0.,1.,0.)))
        return self.times.copy()


def run(args):
    r=P.Runner();model=S.Model(P.OUT/'cost_model')
    before=gpu()
    chains={False:Chain(r,model,False),True:Chain(r,model,True)}
    samples={False:[],True:[]}; passes={False:{},True:{}}
    for index in range(args.samples+16):
        for enabled in ((False,True) if index%2==0 else (True,False)):
            times=chains[enabled].execute(index)
            if index>=16:
                samples[enabled].append(sum(times.values()))
                for name,value in times.items():passes[enabled].setdefault(name,[]).append(value)
        if index%16==15:print('paired samples '+str(index+1-16)+'/'+str(args.samples),flush=True)
    a,b=(np.array(samples[k]) for k in (False,True));delta=b-a
    rng=np.random.default_rng(0x51b)
    starts=rng.integers(0,len(a),size=(2000,max(1,len(a)//16)))
    indices=((starts[:,:,None]+np.arange(16))%len(a)).reshape(2000,-1)
    delta_ci=np.percentile(np.percentile(delta[indices],95,axis=1),[2.5,97.5]).tolist()
    ratio_ci=np.percentile(np.median((b/a)[indices],axis=1),[2.5,97.5]).tolist()
    shared=before!='unavailable' and int(before.split('%')[0].strip())>5
    status='PASS' if delta_ci[1]<=.8 else 'PENDING-QUIET' if shared else 'FAIL'
    cost=dict(renderer=r.gl.renderer,before=before,after=gpu(),samples=samples,
       totals={str(k):dict(p50=float(np.median(v)),p95=float(np.percentile(v,95))) for k,v in samples.items()},
       delta_p50=float(np.median(delta)),delta_p95=float(np.percentile(delta,95)),
       paired_delta_p95_ci=delta_ci,paired_ratio_ci=ratio_ci,
       paired_ratio_p50=float(np.median(b/a)),target_ms=.8,
       status=status,
       passes={str(k):{name:dict(p50=float(np.median(v)),p95=float(np.percentile(v,95))) for name,v in rows.items()}
               for k,rows in passes.items()})
    (P.OUT/'cost.json').write_text(json.dumps(cost,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in cost.items() if k not in ('samples','passes')},indent=2),flush=True)
    return int(cost['status']=='FAIL')


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--worker',action='store_true')
    ap.add_argument('--samples',type=int,default=128);ap.add_argument('--timeout',type=int,default=600)
    args=ap.parse_args()
    if args.worker:return run(args)
    return subprocess.run([sys.executable,__file__,'--worker',*sys.argv[1:]],cwd=P.ROOT,timeout=args.timeout).returncode


if __name__=='__main__':raise SystemExit(main())
