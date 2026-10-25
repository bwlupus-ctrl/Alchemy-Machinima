"""Realistic 1/2/3 m detail, independent acne, deformation and BASE identity.

Production raster/shaders only. The deforming fixture applies a continuous
affine skinning shear to caster triangles, ray intersections, receiver positions
and inverse-transpose normals. No contact/denoise/frustum substitutions.
"""
import copy
import json
import math
from pathlib import Path

import numpy as np

import rig_shadow_gl as R
import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_plan as PL
import relight_3d_cube_source as CS
import relight_world_gl_check as WG


def inside_primitive(prim, points):
    """Exact batched volume test for the independent filter neighbourhood.

    A tangent-plane sample can enter another caster. The scene capsule ray
    helper only returns entry roots, so an origin already inside must also
    count as blocked instead of silently becoming clear.
    """
    if prim.kind == 'obb':
        return np.all(np.abs((points-np.asarray(prim.c))@prim.rot())<np.asarray(prim.h),axis=1)
    if prim.kind == 'sphere':
        delta=points-np.asarray(prim.c)
    else:
        a,b=np.asarray(prim.a),np.asarray(prim.b);axis=b-a
        t=np.clip(((points-a)@axis)/(axis@axis),0.,1.)
        delta=points-(a+t[:,None]*axis)
    return (delta*delta).sum(1)<prim.r*prim.r


def evaluate(defaults,out,shader_ref='WORKTREE'):
    out=Path(out)/'detail';out.mkdir(parents=True,exist_ok=True)
    defaults=dict(defaults)
    assert defaults["RenderSpotShadowDetailBias"] is True, "cold default must activate flagged fixtures"
    r=R.Runner(shader_ref);scene=R.proxy()
    plan=PL.require_compiled(PL.compute_plans_status(r.tree,out)).plans['Default'][2]
    cam=S.Camera((3.5,0,1.65),(0,0,1.2),1920,1080,36)
    base=S.cast_primary(scene,cam);head=np.array([0.,0.,1.67])
    # Freeze the original diagnosed Brow Cut direction/optics independently of
    # preset retuning. Optical setback is measured by the real matrix sampler.
    direction=S.unit(np.array([3.,3.,6.*math.sin(math.pi/4)-.22]))
    rows=[];failures=[];sequences={};static={}
    reference_mesh=WG.mesh;reference_isect=S.isect;reference_normal=S.normal_at;reference_bounds=S._prim_screen_bounds
    probe_positions=base['pos'][::3,::3].reshape(-1,3)
    probe_normals=base['nrm'][::3,::3].reshape(-1,3)
    use=np.isin(base['surf'][::3,::3].ravel(),[1,2,3,4,5])
    # Measure the same continuous material points beside depth/silhouette
    # discontinuities separately; interior averages can hide 2x2 quad crawl.
    edges=np.zeros(base['surf'].shape,bool)
    for axis in (0,1):
        for shift in (-1,1):
            edges |= np.linalg.norm(base['pos']-np.roll(base['pos'],shift,axis),axis=2)>.02
    band=edges.copy()
    for axis in (0,1):
        for shift in range(-4,5):band |= np.roll(edges,shift,axis)
    silhouette_probes=band[::3,::3].ravel()[use]
    probe_positions=probe_positions[use];probe_normals=probe_normals[use]
    def light_at(distance):
        e=dict(pos=head+direction*distance,radius=.01,range=19.8,fov=.2,falloff=.5,
               color=np.ones(3),softness=0.,detail_shadows=True)
        light=S.Light('detail key',e['pos'],.01,19.8,'projector',S.unit(R.CENTRE-e['pos']),math.degrees(.1),seed=S.fnv_seed('rig-shadow-stable-0'))
        return e,light
    def world(cam,d,n,light,detail,master=True):
        r.gpu_atlas=r.raster.atlas(scene,light,.135)
        ch=dict(row=0,near=.135,far=light.range,r_map=light.radius,
                contact=CS.contact_distance(light.range),seed=light.seed,owned=True,
                contact_fade=0.,detail_shadows=detail and master)
        return r.run(cam,d,n,[light],plan,world=dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),channels=[ch]),look=dict(reach=0.))[0]
    def native_contact(cam,d,n,light):
        # Real first-frame/allocation fallback: native sampling plus the
        # shipped area-contact path, with no current world cube ownership.
        ch=dict(row=0,near=.135,far=light.range,r_map=light.radius,
                contact=defaults['RenderAreaShadowMappedContact'],seed=light.seed,
                owned=False,contact_fade=1.)
        return r.run(cam,d,n,[light],plan,world=dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),channels=[ch]),
                     look=dict(strength=defaults['RenderAreaShadowStrength'],contrast=defaults['RenderAreaShadowContrast'],world_amount=defaults['RenderAreaShadowWorldAmount'],reach=defaults['RenderAreaShadowWorldScreenReach'],screen_amount=defaults['RenderAreaShadowScreenAmount']),
                     light_bias=defaults['RenderAreaShadowLightBias'],max_dist=defaults['RenderAreaShadowMaxDist'],thickness=defaults['RenderAreaShadowThickness'])[0]
    def measure(path,D,pose,v,g,e,light,witness,lit):
        delta=e['pos']-g['pos'];ray=delta/np.maximum(np.linalg.norm(delta,axis=2,keepdims=True),1.e-9)
        nl=(g['nrm']*ray).sum(2);grazing=lit&(nl<.25)
        row=dict(path=path,distance_m=D,pose=pose,clear_pixels=int(lit.sum()),
                 acne_false_dark_percent=float((v[lit]<.5).mean()*100) if lit.any() else None,
                 mean_lit_visibility=float(v[lit].mean()) if lit.any() else None,
                 grazing_clear_pixels=int(grazing.sum()),grazing_false_dark_percent=float((v[grazing]<.5).mean()*100) if grazing.any() else None,
                 retention={key:dict(expected=int(witness[key].sum()),retained=int((witness[key]&(v<.5)).sum())) for key in ('nose_brow','jaw')})
        if row['clear_pixels']<100: failures.append(f'{D}m {path} {pose}: insufficient clear receivers')
        elif row['acne_false_dark_percent']>.02: failures.append(f'{D}m {path} {pose}: acne')
        if row['mean_lit_visibility'] is not None and row['mean_lit_visibility']<.98:failures.append(f'{D}m {path} {pose}: partial acne')
        if path.startswith('detail'):
            for key,m in row['retention'].items():
                if m['expected']<30 or m['retained']<.85*m['expected']:failures.append(f'{D}m {path} {pose}: {key} retention')
        rows.append(row)
    try:
        # Three adjacent 60 Hz poses at 0.002 shear/frame: 0.44 mm/frame at
        # the head centre, with continuous movement rather than a pose jump.
        for pose,shear in enumerate((0.,.002,.004)):
            B=np.eye(3);B[0,2]=shear;B[1,2]=shear*.5;inverse=np.linalg.inv(B);origin=np.array([0,0,1.45])
            def transform(points):return (points-origin)@B.T+origin
            def isect(prim,o,d):
                obj_o=(o-origin)@inverse.T+origin
                obj_d=d@inverse.T;scale=np.linalg.norm(obj_d,axis=1)
                return reference_isect(prim,obj_o,obj_d/scale[:,None])/scale
            def normal(prim,p):
                obj=(p-origin)@inverse.T+origin
                n=reference_normal(prim,obj)@inverse
                return n/np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1.e-9)
            WG.mesh=lambda prim:transform(reference_mesh(prim))
            S.isect=isect;S.normal_at=normal;S._prim_screen_bounds=lambda *args:None
            g=S.cast_primary(scene,cam) if shear else base
            _,d,n=S.encode_gbuffer(cam,g)
            points=transform(probe_positions);pn=probe_normals@inverse;pn/=np.linalg.norm(pn,axis=1,keepdims=True)
            for D in (1,2,3):
                e,light=light_at(D)
                ref=r.lighting(cam,d,n,e,0,np.ones((cam.H,cam.W),np.float32),False,defaults)
                witness=R.witnesses(scene,g,light,ref@R.LUMA>.001)
                lit=R.receiver_interiors(scene,g,light)['lit']&np.isin(g['surf'],[1,5])&(ref@R.LUMA>.001)
                r.native_map(scene,light,e['fov'],1920)
                values={}
                for flag in ((False,True) if pose==0 else (True,)):
                    for soft in (False,True):
                        path=('detail' if flag else 'legacy')+('_soft' if soft else '_native')
                        v=r.native_visibility(cam,d,n,e,dict(defaults,RenderSpotShadowDetailBias=flag,BDMergeSoftProjectorShadows=soft))
                        values[path]=v
                        measure(path,D,pose,v,g,e,light,witness,lit)
                contact=native_contact(cam,d,n,light)
                for prefix in (('legacy','detail') if pose==0 else ('detail',)):
                    for suffix in ('native','soft'):
                        path=prefix+'_'+suffix+'_contact'
                        v=np.minimum(values[prefix+'_'+suffix],contact);values[path]=v
                        measure(path,D,pose,v,g,e,light,witness,lit)
                for flag in ((False,True) if pose==0 else (True,)):
                    path='detail_world' if flag else 'legacy_world'
                    v=world(cam,d,n,light,flag);values[path]=v
                    measure(path,D,pose,v,g,e,light,witness,lit)
                xy,visible=R.probes(cam,g,points)
                delta=e['pos']-points;distance=np.linalg.norm(delta,axis=1);ray=delta/distance[:,None]
                clear=np.ones(len(points),bool)
                for prim in scene.prims:clear &= S.isect(prim,points+pn*.0005,ray)>=distance
                for path,v in values.items():
                    if path.startswith('detail'):
                        sequences.setdefault((D,path),[]).append(dict(value=R.sample(v,xy,visible),truth=clear))
                if pose==0 and D==2:
                    static=dict(e=e,light=light,g=g,d=d,n=n,values={k:v.copy() for k,v in values.items()})
    finally:
        WG.mesh=reference_mesh;S.isect=reference_isect;S.normal_at=reference_normal;S._prim_screen_bounds=reference_bounds
    motion=[]
    for (D,path),seq in sequences.items():
        for i,(a,b) in enumerate(zip(seq,seq[1:])):
            # Actual analytic shadow transitions during skinning are physical;
            # measure crawl on material points whose lit/shadow class is stable.
            valid=np.isfinite(a['value'])&np.isfinite(b['value'])&(a['truth']==b['truth'])
            delta=np.abs(b['value'][valid]-a['value'][valid])
            m=dict(distance_m=D,path=path,pair=i,probes=int(valid.sum()),p95=float(np.percentile(delta,95)),pop=float((delta>.25).mean()))
            if m['probes']<100 or m['p95']>R.MOTION_P95 or m['pop']>R.MOTION_POP:failures.append(f'{D}m {path} deform motion')
            edge=valid&silhouette_probes
            edge_delta=np.abs(b['value'][edge]-a['value'][edge])
            m.update(silhouette_probes=int(edge.sum()),silhouette_p95=float(np.percentile(edge_delta,95)),silhouette_pop=float((edge_delta>.25).mean()))
            if edge.sum()<30 or m['silhouette_p95']>R.MOTION_P95 or m['silhouette_pop']>R.MOTION_POP:failures.append(f'{D}m {path} silhouette quad crawl')
            motion.append(m)
    # Compare master-off and unflagged paths with BASE on the exact same GL
    # context, maps and receivers. No per-context texture/program aliasing.
    g=static['g'];d=static['d'];n=static['n'];e=static['e'];light=static['light']
    r.native_map(scene,light,.2,1920)
    vcur=r.native_visibility(cam,d,n,dict(e,detail_shadows=False),defaults)
    vmaster=r.native_visibility(cam,d,n,e,dict(defaults,RenderSpotShadowDetailBias=False))
    vcur_soft=r.native_visibility(cam,d,n,dict(e,detail_shadows=False),dict(defaults,BDMergeSoftProjectorShadows=True))
    vmaster_soft=r.native_visibility(cam,d,n,e,dict(defaults,RenderSpotShadowDetailBias=False,BDMergeSoftProjectorShadows=True))
    wcur=world(cam,d,n,light,False)
    wmaster=world(cam,d,n,light,True,False)
    tree=r.tree;visibility=r.visibility;filt=r.filter
    from rig_shadow_presets_check import BASE
    aircur={}
    for flag in (False,True):
        for soft in (False,True):
            aircur[flag,soft]=r.native_visibility(cam,d,n,dict(e,detail_shadows=flag),dict(defaults,BDMergeSoftProjectorShadows=soft),airborne=True)
    # Actual FIX2 world output must retain exact unflagged/master-off identity.
    # Also exercise those controls through the frozen historical world math,
    # so detail/LOOK code cannot silently redefine its historical oracle.
    world_identity=bool(np.array_equal(wcur,wmaster))
    r.tree=WG.PreLookTree(tree,tree.ref)
    r.filter=r._link('class3/deferred/areaShadowFilterF.glsl')
    historical_unflagged=world(cam,d,n,light,False)
    historical_master=world(cam,d,n,light,True,False)
    # Native/soft/airborne keep the unmodified historical byte oracle. World
    # identity uses frozen 77348de6593 functions, never the current algorithm.
    r.tree=G.SourceTree(BASE)
    r.visibility=r.link_text(r.visibility_source,'BASE detail identity',extra=r.FRAGMENT_COMMON+['deferred/shadowUtil.glsl'],defines='#define SPOT_SHADOW 1\n')
    r.tree=WG.PreLookTree(tree,BASE)
    r.filter=r._link('class3/deferred/areaShadowFilterF.glsl')
    r.tree=G.SourceTree(BASE)
    base_e=dict(e,detail_shadows=False)
    base_defaults=dict(defaults,RenderSpotShadowDetailBias=False)
    vhead=r.native_visibility(cam,d,n,base_e,base_defaults);whead=world(cam,d,n,light,False,False)
    vhead_soft=r.native_visibility(cam,d,n,base_e,dict(base_defaults,BDMergeSoftProjectorShadows=True))
    identity=dict(unflagged_native=bool(np.array_equal(vcur,vhead)),master_off_native=bool(np.array_equal(vmaster,vhead)),
                  unflagged_world=bool(np.array_equal(historical_unflagged,whead)),production_world_master_off=world_identity)
    identity['master_off_world']=bool(np.array_equal(historical_master,whead))
    light_ray=np.asarray(light.pos)-g['pos']
    nl=(light_ray*g['nrm']).sum(2)/np.maximum(np.linalg.norm(light_ray,axis=2),1.e-9)
    # Select unchanged interiors from independent geometry, not from the
    # candidate. The skin oracle intentionally excludes floor (surface 0) for
    # acne statistics, so include its independent ray intersections here too.
    truth=R.receiver_interiors(scene,g,light)
    eligible=(truth['lit']&(whead==1.))|(truth['dark']&(whead==0.))
    floor=g['surf']==0;p=g['pos'][floor];normal=g['nrm'][floor]
    ray=np.asarray(light.pos)-p;distance=np.linalg.norm(ray,axis=1);ray/=distance[:,None]
    clear=np.ones(len(p),bool)
    for prim in scene.prims:clear &= S.isect(prim,p+normal*.0005,ray)>=distance
    floor_class=np.zeros(floor.shape,np.uint8);floor_class[floor]=np.where(clear,1,2)
    floor_interior=floor.copy()
    for axis in (0,1):
        for shift in range(-4,5):floor_interior &= np.roll(floor_class,shift,axis)==floor_class
    eligible |= floor_interior&(((floor_class==1)&(whead==1.))|((floor_class==2)&(whead==0.)))
    protected=WG.historical_protected_mask(whead,eligible&(nl>.2),radius=12)
    # A minimum one-texel cube filter plus bilinear support reaches farther
    # than four camera pixels on this proxy. Exclude physical shadow/crease
    # boundaries using an INDEPENDENT receiver-plane neighbourhood of two
    # cube texels (stretched by incidence), not candidate visibility values.
    # This preserves byte protection on actual interiors while permitting
    # the intentional contact/bias corrections at that filter's boundary.
    p=g['pos'][protected];normal=g['nrm'][protected]
    axis=np.zeros_like(normal);axis[:,2]=1.;axis[np.abs(normal[:,2])>.9]=(0,1,0)
    tangent=np.cross(normal,axis);tangent/=np.linalg.norm(tangent,axis=1,keepdims=True)
    bitangent=np.cross(normal,tangent)
    extent=4.*np.max(np.abs(np.asarray(light.pos)-p),axis=1)/CS.USABLE/np.maximum(nl[protected],.2)
    expected=whead[protected]==1.;stable=np.ones(len(p),bool)
    for x,y in ((-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1)):
        q=p+extent[:,None]*(x*tangent+y*bitangent)+normal*.0005
        ray=np.asarray(light.pos)-q;distance=np.linalg.norm(ray,axis=1);ray/=distance[:,None]
        clear=np.ones(len(q),bool)
        for prim in scene.prims:clear &= (S.isect(prim,q,ray)>=distance)&~inside_primitive(prim,q)
        stable &= clear==expected
    protected[protected]=stable
    identity['production_world_historical_interiors']=bool(protected.sum()>1000 and np.array_equal(wcur[protected],whead[protected]))
    historical_pixels=dict(total=int(protected.sum()),shadow=int((protected&(whead==0.)).sum()),
                           changed=int((protected&(wcur!=whead)).sum()))
    identity.update(unflagged_soft=bool(np.array_equal(vcur_soft,vhead_soft)),master_off_soft=bool(np.array_equal(vmaster_soft,vhead_soft)))
    for (flag,soft),current in aircur.items():
        baseline=r.native_visibility(cam,d,n,dict(base_e,detail_shadows=flag),dict(base_defaults,BDMergeSoftProjectorShadows=soft),airborne=True)
        identity[f'airborne_flag_{flag}_soft_{soft}']=bool(np.array_equal(current,baseline))
    r.tree=tree;r.visibility=visibility;r.filter=filt
    if not all(identity.values()):failures.append('unflagged/master-off/airborne BASE byte identity')
    from rig_shadow_grazing_check import evaluate as grazing_check
    grazing=grazing_check(defaults,out/'grazing',shader_ref)
    if not grazing['passed']:failures.append('broad-FOV grazing partial attenuation')
    from rig_shadow_receiver_check import evaluate as receiver_check
    receiver=receiver_check(out/'receiver',shader_ref)
    if not receiver['passed']:failures.append('normal-map orientation/projector apex')
    result=dict(passed=not failures,shader_ref=shader_ref,grazing=grazing,receiver=receiver,rows=rows,motion=motion,base_identity=identity,identity_reference=BASE,failures=failures,
                fixture='24cm head, 20.4mm nose, 10mm brows; affine skinning shear 0/0.002/0.004 at 60 Hz; independently intersected geometry and full production raster',
                world_identity_reference='77348de6593 world functions; current production also compared on independently confirmed unchanged interiors',
                historical_pixels=historical_pixels,
                limits=dict(retention=.85,acne_false_dark_percent=.02,motion_p95=.06,motion_pop=.05))
    (out/'results.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print('[shadow-detail]', 'PASS' if result['passed'] else 'FAIL',failures,flush=True)
    return result


if __name__=='__main__':
    from rig_shadow_presets_check import settings_defaults,ROOT
    import argparse
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref',default='WORKTREE')
    ap.add_argument('--out',default='.tmp/rig_shadow')
    args=ap.parse_args()
    result=evaluate(settings_defaults(),ROOT/args.out,args.shader_ref)
    raise SystemExit(0 if result['passed'] else 1)
