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
    ap.add_argument('--out',default='.tmp/world_seethru/cost')
    ap.add_argument('--draws',type=int,default=50)
    args=ap.parse_args();out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    a=G.AreaRunner(G.SourceTree(args.base));b=G.AreaRunner(G.SourceTree('WORKTREE'),gl=a.gl)
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
    for radius,detail,empty in ((.1,False,False),(.1,True,False),(1.,False,False),(1.,True,False),(.1,False,True),(1.,True,True)):
        for light in lights:light.radius=radius
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
        row=dict(radius=radius,detail=detail,empty=empty,before_ms=old,after_ms=new,
                 ratio=new/old,passed=new<=1.10*old,median=median,samples=times)
        rows.append(row);print('[receiver-cost]',{k:v for k,v in row.items() if k!='samples'},flush=True)
    result=dict(renderer=gl.renderer,base=args.base,draws_per_sample=args.draws,ceiling_ratio=1.10,rows=rows,passed=all(x['passed'] for x in rows))
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return 0 if result['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
