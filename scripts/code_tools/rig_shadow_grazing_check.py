"""Raster/production-GL lit-plane check, including partial attenuation.

Use --shader-ref HEAD to reproduce the pre-fix regression without changing HEAD.
"""
import argparse
import json
import math
from pathlib import Path

import numpy as np
import rig_shadow_gl as R
import relight_3d_scene as S
import relight_3d_plan as PL
import relight_3d_cube_source as CS


def evaluate(defaults, out, shader_ref='WORKTREE'):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    r=R.Runner(shader_ref)
    scene=S.Scene('lit grazing plane',[S.Prim('plane','obb',c=(-.01,0,1.45),h=(.01,4,4))],[],[],{},np.empty((0,3)))
    cam=S.Camera((3.5,0,1.45),R.CENTRE,320,320,12)
    g=S.cast_primary(scene,cam);_,d,n=S.encode_gbuffer(cam,g)
    mask=(g['surf']==0)&(np.abs(g['pos'][...,1])<.12)&(np.abs(g['pos'][...,2]-1.45)<.12)
    assert mask.sum()>100
    plan=PL.require_compiled(PL.compute_plans_status(r.tree,out)).plans['Default'][2]
    rows=[];failures=[]
    for angle in (60,65,70,75,80,85):
        a=math.radians(angle);pos=R.CENTRE+3*np.array([math.cos(a),math.sin(a),0])
        light=S.Light('grazing key',pos,.01,19.8,'projector',S.unit(R.CENTRE-pos),math.degrees(.75),seed=19)
        e=dict(pos=pos,radius=.01,range=19.8,fov=1.5,falloff=.5,color=np.ones(3),softness=0.,detail_shadows=True)
        for res in (1024,1920,2048):
            r.native_map(scene,light,1.5,res)
            for soft in (False,True):
                v=r.native_visibility(cam,d,n,e,dict(defaults,RenderSpotShadowDetailBias=True,BDMergeSoftProjectorShadows=soft))
                row=dict(path='soft' if soft else 'native',angle=angle,resolution=res,mean_visibility=float(v[mask].mean()),false_dark_percent=float((v[mask]<.5).mean()*100))
                rows.append(row)
        r.gpu_atlas=r.raster.atlas(scene,light,.135)
        ch=dict(row=0,near=.135,far=light.range,r_map=light.radius,contact=CS.contact_distance(light.range),seed=light.seed,owned=True,contact_fade=0.,detail_shadows=True)
        v=r.run(cam,d,n,[light],plan,world=dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),channels=[ch]),look=dict(reach=0.))[0]
        rows.append(dict(path='cube',angle=angle,resolution=512,mean_visibility=float(v[mask].mean()),false_dark_percent=float((v[mask]<.5).mean()*100)))
        r.gpu_atlas=None
    for row in rows:
        if row['mean_visibility']<.98 or row['false_dark_percent']>.02:failures.append(row)
    result=dict(passed=not failures,shader_ref=shader_ref,rows=rows,failures=failures,limits=dict(mean_visibility=.98,false_dark_percent=.02))
    (out/'grazing.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('[shadow-grazing]', 'PASS' if result['passed'] else 'FAIL',json.dumps(rows),flush=True)
    return result


if __name__=='__main__':
    from rig_shadow_presets_check import settings_defaults
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--shader-ref',default='WORKTREE');ap.add_argument('--out',default='.tmp/rig_shadow/fix2/grazing')
    args=ap.parse_args()
    raise SystemExit(0 if evaluate(settings_defaults(),args.out,args.shader_ref)['passed'] else 1)
