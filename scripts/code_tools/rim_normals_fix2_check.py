"""Live-view FIX2 regressions and branch-base core-file identity, using WGL.

No viewer build. Production shaders exercise shared tap barriers and the
zero-Smoothness seed permutation; no live acceptance thresholds are relaxed.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import rim_normals_gl_check as R


def seed_pixels(runner,f,program):
    runner.frame(f,smoothness=0)
    runner._begin(program)
    for name,key,unit in (('depthMap','depth',0),('normalMap','norm',1),('rigRimAuxMap','raw',2)):
        runner.sampler(program,name,runner.textures[key][0],unit)
    runner.uniform(program,'screen_res',f['w'],f['h'])
    runner.uniform(program,'rig_rim_aux_params',0,.002,.02,0)
    runner.uniform(program,'rig_rim_quality',0,0,0,0)
    runner._mat(program,b'inv_proj',np.linalg.inv(f['proj']))
    w,h=(f['w']+1)//2,(f['h']+1)//2
    t=runner._texture('zero_seed',6,0x881A,w,h,0x1908,0x1406,None)
    runner.target(t,w,h);runner.gl.draw(4,0,3)
    return runner.pixels(w,h)


def gl_checks(tree,out,require,metrics):
    runner=R.Runner(tree)
    print('GL',runner.gl.renderer,flush=True)
    metrics['barriers']={}
    for w,h in ((384,384),(1920,1080)):
        for barrier in ('depth','actor','subject'):
            f=R.fixture(w,h,mapped='curved')
            foreground=f['ids']>=0
            if barrier in ('actor','subject'):
                foreground &= np.indices((h,w))[1]<w//2
                f['identity']=np.where(foreground,1,2).astype(np.uint8)
            if barrier=='subject':
                f.update(subject_source=2,certify_identity=False)
            a,_,_=runner.frame(f,detail=1)
            normal=f['normal'].copy()
            normal[~foreground]=R.encode(np.broadcast_to(R.unit(np.array([.94,-.2,.25])),(h,w,3)))[~foreground]
            f['normal']=normal
            b,_,_=runner.frame(f,detail=1)
            error=float(np.max(R.angle(a[foreground],b[foreground])))
            metrics['barriers'][f'{h}/{barrier}']=error
            require(error<.03,f'1: {h}p {barrier}: other-side normals do not change foreground, Texture=1 ({error:.6f} deg < .03)')
    old=R.Runner(R.G.SourceTree(R.BASE),runner.gl)
    zero=runner.gl.loc(runner.seed,b'rig_rim_quality')<0
    equal=True
    for f in (R.fixture(),R.plane_fixture(383,385,0,2.5,1)):
        equal &= np.array_equal(seed_pixels(runner,f,runner.seed),seed_pixels(old,f,old.seed))
    host=tree.read('indra/newview/pipeline.cpp')
    require(zero and equal and 'smooth_normals ? gRigRimSmoothSeedProgram : gRigRimSilSeedProgram' in host,
            '5: zero seed has no active quality uniform and equals baseline fp16 bytes; positive seed has its own program')
    metrics['zero_seed_bytes_equal']=bool(equal)
    metrics['zero_seed_no_quality_uniform']=bool(zero)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--shader-ref',default='WORKTREE')
    p.add_argument('--out',type=Path,default=R.G.REPO/'.tmp/rim_normals/fix2')
    args=p.parse_args();args.out=args.out.resolve();args.out.mkdir(parents=True,exist_ok=True)
    tree=R.G.SourceTree(args.shader_ref); failures=[];metrics={'ref':args.shader_ref}
    def require(ok,message):
        print(('PASS: ' if ok else 'FAIL: ')+message,flush=True)
        if not ok:failures.append(message)
    R.core_identity_checks(tree,require)
    gl_checks(tree,args.out,require,metrics)
    metrics['failures']=failures
    (args.out/'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf-8')
    return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
