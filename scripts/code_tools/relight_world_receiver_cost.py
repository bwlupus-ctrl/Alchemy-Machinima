"""Interleaved 4K GPU A/B of the current world resolve; ceiling is +10%.

Uniform near blockers force the complete production search/filter footprint;
an all-far atlas covers the early-out. Each sample times fifty final draws,
excluding uploads, readbacks, compilation and the unchanged screen gather.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import relight_3d_gl as G
import relight_3d_scene as S


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',default='77348de6593')
    ap.add_argument('--shader-ref',default='WORKTREE')
    ap.add_argument('--out',default='.tmp/world_seethru/cost')
    ap.add_argument('--draws',type=int,default=50)
    ap.add_argument('--gpu-busy',action='store_true',help='known external GPU work: report ceiling misses as PENDING-QUIET, keeping failure exit status')
    args=ap.parse_args();out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    a=G.AreaRunner(G.SourceTree(args.base));b=G.AreaRunner(G.SourceTree(args.shader_ref),gl=a.gl)
    cam=S.Camera((0,0,0),(0,-1,-1),3840,2160,90,.02,16.)
    clip=cam.proj@np.array([0.,0.,-2.,1.])
    depth24=round((clip[2]/clip[3]*.5+.5)*16777215)
    depth=np.full((cam.H,cam.W),round(depth24*4294967295/16777215),np.uint32)
    norm=np.zeros((cam.H,cam.W,4),np.uint16);norm[...,:2]=32768
    lights=[S.Light(str(k),(.1*(2*k-1),0,0),.1,16.,seed=123+k) for k in range(2)]
    plan=dict(samples=8,steps=12,half=True,near_steps=0)
    gl=a.gl
    def sample(r,on):
        r._begin(r.filter)
        gl.uf[4](gl.loc(r.filter,b'area_world'),float(on),float(on),0.,0.)
        stats={}
        def draws():
            for _ in range(args.draws):gl.draw(4,0,3)
        r._timed('upsample',stats,draws)
        return stats['upsample']/args.draws
    rows=[]
    cases=[(radius,detail,empty,False) for radius,detail,empty in
           ((.1,False,False),(.1,True,False),(1.,False,False),(1.,True,False),(.1,False,True),(1.,True,True))]
    cases += [(1.,False,empty,True) for empty in (False,True)]
    for radius,detail,empty,material_tilt in cases:
        for light in lights:light.radius=radius
        if material_tilt:
            # The geometry is a full-resolution constant-depth plane with its
            # light behind it at grazing incidence. A 25-degree material tilt
            # toward that light must NOT turn geometric rejection into PCSS.
            view_normal=np.array([0.,np.sin(np.radians(25)),np.cos(np.radians(25))])
            norm[...,:2]=np.round(S.oct_encode(view_normal[None,:])[0]*65535).astype(np.uint16)
            for k,light in enumerate(lights):
                light.pos=cam.eye+cam.view[:3,:3].T@np.array([.1*(2*k-1),1.,-2.025])
        else:
            norm[...,:2]=32768
            for k,light in enumerate(lights):light.pos=(.1*(2*k-1),0,0)
        atlas=np.full((2176,1632),1. if empty else 0.,np.float32)
        channels=[dict(row=2*k,near=.02,far=16.,r_map=radius,contact=.0,seed=123+k,detail_shadows=detail) for k in range(2)]
        for r in (a,b):
            r.run(cam,depth,norm,lights,plan,denoise=0.,world=dict(atlas=atlas,channels=channels))
        for _ in range(4):sample(a,True);sample(b,True)
        times={key:[] for key in ('a_on','b_on','a_off','b_off')}
        for pair in range(12):
            order=[('a',a),('b',b)] if pair%2==0 else [('b',b),('a',a)]
            for name,r in order:
                times[name+'_off'].append(sample(r,False))
                times[name+'_on'].append(sample(r,True))
        median={key:float(np.median(values)) for key,values in times.items()}
        old=median['a_on']-median['a_off'];new=median['b_on']-median['b_off']
        paired=[dict(before_ms=ao-af,after_ms=bo-bf) for ao,af,bo,bf in
                zip(times['a_on'],times['a_off'],times['b_on'],times['b_off'])]
        row=dict(radius=radius,detail=detail,empty=empty,material_tilt_deg=25 if material_tilt else 0,before_ms=old,after_ms=new,
                 ratio=new/old if old>0 else None,passed=old>0 and new>=0 and new<=1.10*old,median=median,samples=times,paired=paired)
        rows.append(row);print('[receiver-cost]',{k:v for k,v in row.items() if k not in ('samples','paired')},flush=True)
    # Preserve the ceiling and paired data even during the user's live GPU
    # recording. A noisy miss is pending quiet certification, never a pass.
    passed=all(x['passed'] for x in rows)
    result=dict(renderer=gl.renderer,base=args.base,shader_ref=args.shader_ref,draws_per_sample=args.draws,ceiling_ratio=1.10,rows=rows,passed=passed,
                external_gpu_busy=args.gpu_busy,status='PASS' if passed else 'PENDING-QUIET' if args.gpu_busy else 'FAIL')
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return 0 if result['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
