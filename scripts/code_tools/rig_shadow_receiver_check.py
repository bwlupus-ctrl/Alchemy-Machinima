"""Production receiver math: normal-map orientation and projector apex guards."""
import argparse
import json
from pathlib import Path

import numpy as np
import rig_shadow_gl as R
import relight_3d_scene as S


def evaluate(out, shader_ref='WORKTREE'):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    r=R.Runner(shader_ref);cam=S.Camera((0,0,0),(0,0,-1),4,4,90,.135,20)
    pr=r.link_text('''out vec4 frag_color;uniform vec3 fixture_normal;uniform float fixture_z;
vec4 detailSpotReceiver(vec3 p,vec3 n,int i);
void main(){vec3 p=vec3((gl_FragCoord.xy-2.0)*.001,fixture_z);frag_color=detailSpotReceiver(p,fixture_normal,0);}
''','receiver guard',extra=r.FRAGMENT_COMMON+['deferred/shadowUtil.glsl'],defines='#define SPOT_SHADOW 1\n')
    bias=np.eye(4);bias[:3,:3]*=.5;bias[:3,3]=.5
    def run(z,normal):
        r.target_float(cam);r._begin(pr)
        r._mat(pr,b'shadow_matrix[4]',bias@cam.proj)
        r.uniform(pr,'proj_shadow_res',1920,1920);r.uniform(pr,'fixture_normal',*normal)
        r.uniform(pr,'fixture_z',z);r.uniform(pr,'spot_shadow_bias',-.0005)
        return r.read_float(cam)
    a=run(-3,(0,0,1));b=run(-3,(.99,0,-.1))
    checks=dict(normal_map_orientation=bool(np.array_equal(a,b)))
    y,x=np.mgrid[:4,:4]
    for z in (0.,1.,-1.e-6):
        points=np.stack(((x+.5-2)*.001,(y+.5-2)*.001,np.full((4,4),z),np.ones((4,4))),axis=2)
        expected=points@(bias@cam.proj).T
        try:
            actual=run(z,(0,0,1))
            checks['apex_'+str(z)]=bool(np.isfinite(actual).all() and np.max(np.abs(actual-expected))<1.e-7)
        except AssertionError:checks['apex_'+str(z)]=False
    result=dict(passed=all(checks.values()),shader_ref=shader_ref,checks=checks,normal_map_projection_delta=float(np.max(np.abs(a-b))))
    (out/'receiver.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('[shadow-receiver]',result,flush=True)
    return result


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--shader-ref',default='WORKTREE');ap.add_argument('--out',default='.tmp/rig_shadow/fix2/receiver')
    args=ap.parse_args();raise SystemExit(0 if evaluate(args.out,args.shader_ref)['passed'] else 1)
