"""Exhaustive OFF-chain production-shader fixture vs 3efef2be20a.

34,560 full frames: HDR/non-HDR/S-Log3 x grade x all Roto layers x
FXAA/SMAA/off x CAS x motion blur x DOF x RLVa x diopter/kaleido/off x
fisheye x vignette x film grain. Prefix traversal shares identical earlier
passes, but every final combination is drawn and compared byte-for-byte.
Scene/GBuffer inputs are fixtures; no viewer or private reference files.
"""
import os
os.environ['PYTHONUTF8']='1'
os.environ['PYTHONDONTWRITEBYTECODE']='1'
import argparse
import itertools
import json
import subprocess
import sys
import numpy as np
import sensor_post_gl_check as P
from sensor_post_cost_check import lookup


def run(args):
    P.OUT.mkdir(parents=True,exist_ok=True)
    r=P.Runner()
    w,h=24,16
    yy,xx=np.mgrid[0:h,0:w]
    image=np.ones((h,w,4),np.float32)
    image[:,:,:3]=np.stack([xx/w,yy/h,((xx+yy)%7)/6],axis=2)*2
    scene=r.upload('legacy-scene',image)
    depth=image.copy();depth[:,:,:3]=np.where((xx>w//2)[:,:,None],.7,.3)
    depthtex=r.upload('legacy-depth',depth)
    norm=image.copy();norm[:,:,:3]=(.5,.5,1.);norm[:,:,3]=.34
    normtex=r.upload('legacy-normal',norm)
    one=r.upload('legacy-one',np.ones_like(image))
    bloom=r.upload('legacy-bloom',image*.03)
    velocity=image.copy();velocity[:,:,:3]=(.1,0.,0.)
    vel=r.upload('legacy-velocity',velocity)
    area=lookup(r,'SMAAAreaTex.h',160,560,2);search=lookup(r,'SMAASearchTex.h',64,16,1)
    roots=[];programs={};defaults={};temporary={}
    def program(file,defs='',modules=(),vertex=None,ref=None):
        if vertex and not vertex.startswith('class'): vertex='class1/'+vertex
        return r.program('class1/'+file,defs,modules,vertex,ref=ref)
    refs=(None,P.BASELINE)
    common=P.common_modules()
    for ref in refs:
        pr={}
        pr['copy']=program('deferred/postDeferredNoDoFF.glsl',modules=common,ref=ref)
        pr['roto']=program('deferred/cineOutlineF.glsl',modules=common,ref=ref)
        pr['blur']=program('deferred/motionBlurF.glsl',ref=ref)
        pr['cof']=program('deferred/cofF.glsl',modules=common,ref=ref)
        pr['dof']=program('deferred/postDeferredF.glsl','#define FRONT_BLUR 1\n',common,ref=ref)
        pr['combine']=program('deferred/dofCombineF.glsl',ref=ref)
        pr['rlv']=program('deferred/rlvF.glsl',modules=common,ref=ref)
        pr['dio']=program('deferred/ultimateDiopterGatherF.glsl','#define DIOPTER_TAPS 24\n',common,ref=ref)
        pr['dio_comp']=program('deferred/ultimateDiopterF.glsl',modules=common,ref=ref)
        pr['kal']=program('deferred/ultimateKaleidoF.glsl',modules=common,ref=ref)
        pr['fish']=program('deferred/cineFisheyeF.glsl',ref=ref)
        pr['cas']=program('deferred/CASF.glsl',modules=('class1/environment/srgbF.glsl',),ref=ref)
        pr['present']=program('alchemy/blitWithEffectsF.glsl',modules=common+('class1/alchemy/postEffectUtilsF.glsl',),ref=ref)
        pr['fxaa_encode']=program('interface/glowcombineFXAAF.glsl',modules=common,
            vertex='interface/glowcombineFXAAV.glsl',ref=ref)
        pr['fxaa']=program('deferred/fxaaF.glsl','#define FXAA_GLSL_400 1\n#define FXAA_QUALITY__PRESET 28\n',
            vertex='class1/deferred/postDeferredV.glsl',ref=ref)
        defs='#define SMAA_GLSL_4 1\n#define SMAA_PRESET_HIGH 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n'
        mods=('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl')
        for key,name in [('edge','EdgeDetect'),('weights','BlendWeights'),('smaa','NeighborhoodBlend')]:
            pr[key]=program('deferred/SMAA'+name+'F.glsl',defs,mods,'class1/deferred/SMAA'+name+'V.glsl',ref)
        programs[ref]=pr
        temporary[ref]={name:r.target(str(ref)+name,w,h,count) for name,count in
             [('cof',1),('gather',1),('edge',1),('weights',1),('encoded',1),('lens',3),('roto',1)]}
    base=dict(exposure=2.,tonemap_type=3,tonemap_mix=1.,tonemap_grade=(1.,1.),tonemap_params=(36.,0.,0.,0.),
        bloom_strength=.325,ca_amount=.002,ca_falloff=1.,ca_angle_sin_cos=(0.,1.),ca_offset_r=(-1.,0.),ca_offset_b=(1.,0.),
        uWhiteBalanceGain=(1.,1.,1.),uLift=(0.,0.,0.),uGain=(1.,1.,1.),uInvGammaCC=(1.,1.,1.),
        uBWPScale=1.,uBCScale=1.,uSaturation=1.,uCurveInvRange=(1.,1.,1.))
    roto=dict(outline_color=(.3,.2,.8),outline_params=(1.5,.5,0.,2.),outline_params2=(0.,0.,0.,0.),
        roto_ink=(.8,2.,1.,0.),roto_line=(.02,.134,1.,1.),roto_subject=(0.,0.,0.,0.),roto_subject3=(0.,0.,0.,0.))
    shape=dict(diopter_shape=(.5,.5,.4,1.),diopter_shape2=(0.,.06,0.,0.),diopter_shape3=(0.,0.,0.,0.),
        diopter_focus=(8.,1.5,.4,6.),diopter_focus2=(1.,1.,1.,4.),diopter_focus3=(4.,0.,.5,1.),
        diopter_aperture=(0.,6.,0.,1.),diopter_aperture2=(.45,1.,0.,0.))
    metrics={'SMAA_RT_METRICS':(1/w,1/h,w,h)}
    cas=dict(out_screen_res=(w,h),cas_param_0=np.array([1.,1.,0.,0.],np.float32).view(np.uint32),
        cas_param_1=np.array([-1./6.8,0.,0.,0.],np.float32).view(np.uint32))
    count=0;nodes=0;failures=[];activity={name:False for name in ('aa','cas','blur','dof','rlv','lens','fish','roto','vignette','grain')}
    def draw(ref,key,dst,tex,uniforms=None):
        return r.draw(programs[ref][key],dst,w,h,tex,uniforms)[0]
    def effect(ref,name,value,src,dst):
        temp=temporary[ref]
        if name=='aa':
            if value==1:
                draw(ref,'fxaa_encode',temp['encoded'],[('diffuseRect',src)])
                return draw(ref,'fxaa',dst,[('diffuseMap',temp['encoded'][0]),('depthMap',depthtex)],
                    {'tc_scale':(1.,1.),'rcp_screen_res':(1/w,1/h)})
            draw(ref,'edge',temp['edge'],[('diffuseRect',src)],metrics)
            draw(ref,'weights',temp['weights'],[('edgesTex',temp['edge'][0]),('areaTex',area),('searchTex',search)],metrics)
            return draw(ref,'smaa',dst,[('diffuseRect',src),('blendTex',temp['weights'][0])],metrics)
        if name=='cas':return draw(ref,'cas',dst,[('diffuseRect',src)],cas)
        if name=='blur':return draw(ref,'blur',dst,[('diffuseRect',src),('velocityMap',vel)],{'motion_blur_strength':8})
        if name=='dof':
            draw(ref,'cof',temp['cof'],[('diffuseRect',src),('depthMap',depthtex)],
                dict(focal_distance=-8.,blur_constant=.002,tan_pixel_angle=.0002,magnification=1.,max_cof=3.,res_scale=1.))
            draw(ref,'dof',temp['gather'],[('diffuseRect',temp['cof'][0])],dict(max_cof=3.,res_scale=1.))
            return draw(ref,'combine',dst,[('diffuseRect',temp['gather'][0]),('lightMap',temp['cof'][0])],
                dict(max_cof=3.,res_scale=1.,dof_width=(w-1)/w,dof_height=(h-1)/h))
        if name=='rlv':return draw(ref,'rlv',dst,[('diffuseRect',src),('depthMap',depthtex)],dict(
            rlvEffectMode=0,rlvEffectParam1=(0.,0.,0.,1.),rlvEffectParam2=(.5,0.,.5,100.),rlvEffectParam4=(.2,.4,.8,0.)))
        if name=='lens':
            if value==1:
                draw(ref,'dio',temp['lens'],[('diffuseRect',src),('depthMap',depthtex)],shape)
                return draw(ref,'dio_comp',dst,[('diffuseRect',temp['lens'][0]),('emissiveRect',temp['lens'][1]),
                    ('diffuseMap',src),('depthMap',depthtex)],dict(shape,diopter_comp=(0.,.5,.01,.1),diopter_comp2=(0.,0.,1.,0.)))
            draw(ref,'kal',temp['lens'],[('diffuseRect',src),('depthMap',depthtex)],dict(
                kal_pattern=(0.,6.,1.,0.),kal_shape=(.2,0.,0.,0.),kal_center=(.5,.5,.5,.5),
                kal_source=(0.,1.,0.,0.),kal_look=(0.,1.,0.,0.),kal_protect=(0.,0.,0.,0.)))
            return draw(ref,'copy',dst,[('diffuseRect',temp['lens'][0])])
        if name=='fish':return draw(ref,'fish',dst,[('diffuseMap',src)],dict(
            fisheye_params=(1.2,.3,.92,.18),fisheye_params2=(0.,0.,1.,0.)))
        raise AssertionError(name)
    features=[('aa',3),('cas',2),('blur',2),('dof',2),('rlv',2),('lens',3),('fish',2)]
    targets={ref:[r.target('legacy-'+str(ref)+str(i),w,h,fmt=0x8059) for i in range(len(features)+2)] for ref in refs}
    def walk(level,srcs,path):
        nonlocal count,nodes
        if level==len(features):
            plain=None
            for vignette,grain in itertools.product(range(2),repeat=2):
                arrays=[]
                for ref,src in zip(refs,srcs):
                    arrays.append(draw(ref,'present',targets[ref][-1],[('diffuseRect',src),('depthMap',depthtex)],dict(
                        uVignetteAmount=.3*vignette,uVignetteRadius=1.,uVignetteSoft=.5,uVignetteFeather=1.,
                        uVignetteAspect=(1.,1.),uVignetteColor=(.1,.04,.2),uGrainAmount=.12*grain,
                        uGrainStyle=1,uGrainSize=1.,uGrainRange=.5,uGrainTint=(1.,1.,1.),uGrainAnimate=0)))
                count+=1
                if not vignette and not grain: plain=arrays[0]
                if grain and not vignette and not np.array_equal(plain,arrays[0]):activity['grain']=True
                if vignette and not grain and not np.array_equal(plain,arrays[0]):activity['vignette']=True
                if not np.array_equal(arrays[0].view(np.uint32),arrays[1].view(np.uint32)):
                    failures.append((root,path,vignette,grain));raise AssertionError(failures[-1])
                if count%4096==0:print(str(count)+' full-frame OFF combinations identical',flush=True)
            return
        name,choices=features[level]
        for value in range(choices):
            nextsrc=[]
            if value==0:nextsrc=srcs
            else:
                arrays=[]
                for ref,src in zip(refs,srcs):
                    arrays.append(effect(ref,name,value,src,targets[ref][level]));nextsrc.append(targets[ref][level][0])
                nodes+=1
                assert np.array_equal(arrays[0].view(np.uint32),arrays[1].view(np.uint32)),(root,path,name,value)
                if not activity[name]:
                    before=draw(None,'copy',temporary[None]['roto'],[('diffuseRect',srcs[0])])
                    activity[name]=not np.array_equal(before,arrays[0])
            walk(level+1,nextsrc,path+[(name,value)])
    for mode,grade,layer in itertools.product(range(3),range(2),range(5)):
        root=(mode,grade,layer);srcs=[]
        for ref in refs:
            source=scene
            if layer in (1,2,3):
                ink=draw(ref,'roto',temporary[ref]['roto'],[('rotoScene',source),('depthMap',depthtex),('normalMap',normtex),('exposureMap',one)],
                    dict(roto,exposure=0. if layer==1 else 2.,roto_ink=(.8,2.,0.,0.)))
                if not ref and np.max(abs(ink[:,:,:3]-image[:,:,:3]))>.01:activity['roto']=True
                source=temporary[ref]['roto'][0]
            defs='#define HAS_POST_EFFECTS 1\n#define BLOOM_COMPOSITE 1\n'
            mods=common+('class1/alchemy/postEffectUtilsF.glsl',)
            if mode!=0:
                defs+='#define TONEMAP 1\n';mods+=('class1/deferred/tonemapUtilF.glsl','class1/deferred/LPMUtil.glsl')
            if grade:defs+='#define COLOR_GRADE 1\n';mods+=('class1/alchemy/colorGradeUtilF.glsl',)
            pr=program('alchemy/colorCorrectF.glsl',defs,mods,ref=ref)
            dst=temporary[ref]['encoded']
            r.draw(pr,dst,w,h,[('diffuseRect',source),('depthMap',depthtex),('bloomMap',bloom),('exposureMap',one)],
                dict(base,tonemap_type=8 if mode==2 else 3,tonemap_params=(0.,0.,0.,0.) if mode==2 else (36.,0.,0.,0.)))
            source=dst[0]
            if layer==4:
                draw(ref,'roto',temporary[ref]['roto'],[('rotoScene',source),('depthMap',depthtex),('normalMap',normtex)],
                    dict(roto,exposure=0.));source=temporary[ref]['roto'][0]
            # Root storage must survive FXAA's RGBL encoder below.
            draw(ref,'copy',targets[ref][-2],[('diffuseRect',source)])
            srcs.append(targets[ref][-2][0])
        walk(0,srcs,[])
    assert all(activity.values()),activity
    result=dict(status='PASS',full_frames=count,intermediate_comparisons=nodes,reference=P.BASELINE,activity=activity,
                renderer=r.gl.renderer,size=[w,h],failures=failures,scope=__doc__)
    (P.OUT/'legacy.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='scope'},indent=2),flush=True)
    return 0


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--worker',action='store_true')
    ap.add_argument('--timeout',type=int,default=1200);args=ap.parse_args()
    if args.worker:return run(args)
    return subprocess.run([sys.executable,__file__,'--worker',*sys.argv[1:]],cwd=P.ROOT,timeout=args.timeout).returncode


if __name__=='__main__':raise SystemExit(main())
