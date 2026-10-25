"""Second-review predicates, production GLSL with read-only baseline mutations."""
import argparse
import json
import os
import subprocess
import sys
import numpy as np
import sensor_post_gl_check as P


def tests(r, check):
    dispatch=(P.ROOT/'scripts/code_tools/sensor_post_dispatch_check.py').read_text(encoding='utf-8')
    if r.reference:
        dispatch=subprocess.check_output(['git','show',r.reference+':scripts/code_tools/sensor_post_dispatch_check.py'],cwd=P.ROOT,text=True,encoding='utf-8',timeout=15)
    extraction=dispatch[dispatch.index("methods="):dispatch.index('# The no-post')]
    check('dispatch extracts all three production finalize helpers',all("'"+n+"'" in extraction for n in ('renderFinalizeScene','renderFinalizeLegacy','renderFinalizePresent')))
    w,h=96,80
    out=r.target('fix2-out',w,h,fmt=0x8814)
    rows=[]; occlusion=[]; additive_errors=[]
    for front in (0,1):
        defs='#define AL_SENSOR_HDR 1\n#define FRONT_BLUR '+str(front)+'\n'
        reduce=r.program('class1/deferred/postDeferredF.glsl',defs+'#define AL_SENSOR_DOF_REDUCE 1\n',P.common_modules())
        gather=r.program('class1/deferred/postDeferredF.glsl',defs,P.common_modules())
        combine=r.program('class1/deferred/dofCombineF.glsl',defs)
        cells=r.target('fix2-cells',w,h);blur=r.target('fix2-blur',w,h)
        for scale in (1.,.7,.5):
            ww,hh=int(w*scale),int(h*scale)
            u=dict(screen_res=(w,h),max_cof=10.,res_scale=scale,dof_width=(ww-1)/w,dof_height=(hh-1)/h)
            for pair in ((.5,0.),(0.,.5),(.5,1.),(1.,.5),(0.,1.),(1.,0.)):
                image=np.ones((h,w,4),np.float32)
                image[:,:w//2,3]=pair[0];image[:,w//2:,3]=pair[1]
                src=r.upload('fix2-split',image)
                r.draw(reduce,cells,ww,hh,[('diffuseRect',src)],u,read=False)
                r.draw(gather,blur,ww,hh,[('diffuseRect',cells[0])],u,read=False)
                result=r.draw(combine,out,w,h,[('diffuseRect',blur[0]),('lightMap',src)],u)[0]
                rows.append(dict(front=front,scale=scale,pair=pair,error=float(abs(result[:,:,:3]-1).max())))
                if not r.reference and pair==(.5,0.):
                    additive=r.program('class1/deferred/dofCombineF.glsl',defs,ref='9f1bb3b6d67')
                    wrong=r.draw(additive,out,w,h,[('diffuseRect',blur[0]),('lightMap',src)],u)[0]
                    additive_errors.append(float(abs(wrong[:,:,:3]-1).max()))
            # A bright far background must not illuminate the focused foreground.
            image[:,:,:3]=0.;image[:,:w//2,3]=.5;image[:,w//2:,3]=0.;image[:,w//2:,:3]=16.
            src=r.upload('fix2-occlusion',image)
            r.draw(reduce,cells,ww,hh,[('diffuseRect',src)],u,read=False)
            r.draw(gather,blur,ww,hh,[('diffuseRect',cells[0])],u,read=False)
            result=r.draw(combine,out,w,h,[('diffuseRect',blur[0]),('lightMap',src)],u)[0]
            occlusion.append(float(abs(result[:,:w//2,:3]).max()))
    check('heterogeneous focused/near/far radiance both orders at 1/.7/.5',max(row['error'] for row in rows)<.002,rows)
    if not r.reference:
        check('mutation restoring only old additive combine creates brightness seams',min(additive_errors)>.05,additive_errors)
    check('opaque focused foreground rejects background scatter',max(occlusion)<.002,occlusion)
    path='class1/deferred/postDeferredF.glsl'
    source=r.source(path)
    def tap_probe(text):
        return text.replace('vec4 sensorDof()', 'int probe_taps = 0;\nvec4 sensorDof()').replace(
            'vec4 source = texelFetch', '++probe_taps; vec4 source = texelFetch').replace(
            'frag_color = sensorDof();','frag_color = sensorDof(); frag_color = vec4(float(probe_taps));')
    defs='#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 0\n'
    image=np.random.default_rng(26).uniform(0,16,(h,w,4)).astype(np.float32);image[:,:,3]=.5
    src=r.upload('fix2-focused-cost',image);reduced=r.target('fix2-focused-reduced',w,h)
    reduce=r.program(path,defs+'#define AL_SENSOR_DOF_REDUCE 1\n',P.common_modules())
    u=dict(screen_res=(w,h),max_cof=10.,res_scale=.7)
    r.draw(reduce,reduced,int(w*.7),int(h*.7),[('diffuseRect',src)],u,read=False)
    probe=r.program(path,defs,P.common_modules(),mutation=(source,tap_probe(source)))
    taps=r.draw(probe,out,int(w*.7),int(h*.7),[('diffuseRect',reduced[0])],u)[0]
    check('focused shipped-cap gather performs zero disk taps',float(taps[:,:,0].max())==0.,dict(maximum_disk_fetches=float(taps[:,:,0].max())))
    if not r.reference:
        no_early=source.replace('if (center_coc >= -0.5) return center;','')
        mutant=r.program(path,defs,P.common_modules(),mutation=(source,tap_probe(no_early)))
        wrong=r.draw(mutant,out,int(w*.7),int(h*.7),[('diffuseRect',reduced[0])],u)[0]
        check('mutation without focused early-out restores 149 disk taps',float(wrong[:,:,0].min())==149.)
        optimized=r.program(path,defs,P.common_modules())
        equivalent=r.program(path,defs,P.common_modules(),mutation=(source,no_early))
        outputs=[r.draw(pr,out,int(w*.7),int(h*.7),[('diffuseRect',reduced[0])],u)[0] for pr in (optimized,equivalent)]
        check('focused early-out is bit-identical to complete gather',np.array_equal(*outputs))
    # The production Stage C and godray converter use the same selected grade.
    identity=dict(uWhiteBalanceGain=(1.,1.,1.),uLift=(0.,0.,0.),uGain=(1.,1.,1.),uInvGammaCC=(1.,1.,1.),
        uBWPScale=1.,uBCScale=1.,uSaturation=1.,uCurveInvRange=(1.,1.,1.),gamma=1.)
    results=[]
    for name,grade,legacy,extra in (
        ('linear gain',True,False,dict(uGain=(2.,2.,2.))),
        ('white balance LGG display',True,False,dict(uWhiteBalanceGain=(1.1,1.,.9),uLift=(.01,.02,.005),uInvGammaCC=(.95,1.05,1.1),uBCScale=.9,uBCBias=.03,uSaturation=.8)),
        ('legacy gamma',True,True,dict(gamma=.8,uGain=(1.2,.9,1.1))),
        ('coupled tone saturation',False,False,dict(tonemap_grade=(.5,1.))),
    ):
        defs='#define TONEMAP 1\n'+('#define COLOR_GRADE 1\n' if grade else '')+('#define LEGACY_GAMMA 1\n' if legacy else '')
        modules=P.common_modules()+('class1/deferred/tonemapUtilF.glsl','class1/deferred/LPMUtil.glsl')+(('class1/alchemy/colorGradeUtilF.glsl',) if grade else ())
        ray=r.program('class1/alchemy/colorCorrectF.glsl',defs+'#define AL_SENSOR_GODRAYS 1\n',modules)
        camera=r.program('class1/alchemy/colorCorrectF.glsl',defs+'#define AL_SENSOR_CAMERA 1\n',modules)
        u=dict(identity,tonemap_type=3,tonemap_mix=1.,tonemap_grade=(1.,1.),tonemap_params=(36.,0.,0.,0.));u.update(extra)
        image=np.ones((h,w,4),np.float32);image[:,:,:3]=(.05,.035,.02)
        src=r.upload('fix2-scene',image,fmt=0x8814)
        base=r.draw(camera,out,w,h,[('diffuseRect',src)],u)[0]
        image[:,:,:3]=(.1,.07,.03);increment=r.upload('fix2-ray',image,fmt=0x8814)
        converted=r.draw(ray,out,w,h,[('diffuseRect',src),('diffuseMap',increment)],u)[0]
        tex=r.upload('fix2-converted',converted,fmt=0x8814)
        final=r.draw(camera,out,w,h,[('diffuseRect',tex)],u)[0]
        error=float(abs(final[:,:,:3]-np.minimum(base[:,:,:3]+image[:,:,:3],1.)).max())
        results.append(dict(name=name,error=error,actual=final[0,0,:3].tolist(),target=(base[0,0,:3]+image[0,0,:3]).tolist()))
    check('godray inversion includes selected WB/LGG/display/legacy grade',max(row['error'] for row in results[:3])<.001,results[:3])
    check('godray coupled saturation reaches coloured target',results[3]['error']<.001,results[3])
    # Observe uniforms submitted by the actual benchmark, rather than trusting
    # its report labels. The old fixture silently measured cap 4 instead of 10.
    import sensor_post_cost_check as cost
    import types
    fixture=cost
    if r.reference:
        source=subprocess.check_output(['git','show',r.reference+':scripts/code_tools/sensor_post_cost_check.py'],cwd=P.ROOT,text=True,encoding='utf-8',timeout=15)
        fixture=types.ModuleType('baseline_cost');exec(compile(source,'baseline_cost','exec'),fixture.__dict__)
    model=P.S.Model(P.OUT/'fix2-cost-model')
    observed=[];original=r.draw
    def draw(*args,**kwargs):
        uniforms=args[5] if len(args)>5 else kwargs.get('uniforms')
        if uniforms and 'max_cof' in uniforms:observed.append(float(uniforms['max_cof']))
        return original(*args,**kwargs)
    r.draw=draw
    try:
        # Use current GLSL so this predicate isolates benchmark configuration.
        reference=r.reference;r.reference=None
        chain=fixture.Chain(r,model,True,w=96,h=80);chain.execute(0)
        r.reference=reference
    finally:r.draw=original
    shipped,maximum=cost.dof_caps()
    check('cost fixture submits shipped cap rather than cap four',bool(observed) and all(v==shipped for v in observed),dict(shipped=shipped,largest_supported=maximum,observed=observed))
    if not r.reference:
        observed=[];r.draw=draw
        try:
            chain=cost.Chain(r,model,True,w=96,h=80,max_cof=maximum);chain.execute(0)
        finally:r.draw=original
        check('cost fixture submits largest supported cap',bool(observed) and all(v==maximum for v in observed),observed)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--reference');ap.add_argument('--worker',action='store_true');ap.add_argument('--timeout',type=int,default=240)
    args=ap.parse_args()
    if not args.worker:
        return subprocess.run([sys.executable,__file__,'--worker',*sys.argv[1:]],cwd=P.ROOT,
            env=dict(os.environ,PYTHONUTF8='1'),timeout=args.timeout).returncode
    r=P.Runner(args.reference);rows=[]
    def check(name,passed,details=None):
        rows.append(dict(name=name,passed=bool(passed),details=details));print(('PASS ' if passed else 'FAIL ')+name,flush=True)
    tests(r,check)
    (P.OUT/('fix2_before.json' if args.reference else 'fix2_after.json')).write_text(json.dumps(rows,indent=2),encoding='utf-8')
    return int(any(not row['passed'] for row in rows))


if __name__=='__main__':raise SystemExit(main())
