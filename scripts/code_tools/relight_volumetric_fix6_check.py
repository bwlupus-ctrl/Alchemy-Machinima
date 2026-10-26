"""FIX6 adversarial shadow maps, distinct projectors and fine boundary walks.

This proves zero *additional* discontinuity relative to Full, not mathematical
continuity of Full's discrete sample-count controls, PCF or RGBA16F output.
"""
import copy
import json
import numpy as np
import relight_volumetric_gl_check as V

BLOCKERS = ('none','fence','slats','leaves')
STEP_ENERGY_BOUND = .005  # 0.5% of unshadowed Full energy per fine step


def light_geometry(original, position=None, direction=None, fov=None):
    light = dict(original)
    pos = np.array(original['pos'] if position is None else position)
    axis = V.S.unit(original['dir'] if direction is None else direction)
    fov = original['fov'] if fov is None else fov
    lc = V.S.Camera(pos,pos+axis*4,256,256,vfov=np.rad2deg(fov),near=.15,far=12)
    g = V.S.cast_primary(light['scene'],lc)
    zf = np.where(np.isfinite(g['zf']),g['zf'],12)
    z = (12+.15)/(12-.15)-2*12*.15/((12-.15)*zf)
    bias=np.eye(4);bias[:3,:3]*=.5;bias[:3,3]=.5
    vp=lc.proj@lc.view
    light.update(pos=pos,dir=axis,view=lc.view,fov=fov,matrix=bias@vp,
        planes=np.array([vp[3]+vp[0],vp[3]-vp[0],vp[3]+vp[1],vp[3]-vp[1],vp[3]+vp[2],vp[3]-vp[2]]),
        depth=np.ascontiguousarray(np.clip(z*.5+.5,0,1),np.float32))
    return light


def blockers(light, kind, seed=0):
    """Depth-tested light-space planes: fine opaque bars, slats or leaf cutouts.

    Receiver geometry remains fixed while blocker depth/pitch/orientation/seed
    vary. These are shadow-map details, never a post-render image mask.
    """
    light=dict(light)
    h,w=light['depth'].shape; y,x=np.mgrid[:h,:w]
    rng=np.random.default_rng(seed)
    pitch=int(rng.integers(7,17)); phase=int(rng.integers(pitch))
    if kind=='none':
        light['depth']=np.ones((h,w),np.float32)
        return light
    if kind=='fence': mask=((x+phase)%pitch<max(2,pitch//3)) | ((y+2*phase)%47<3)
    elif kind=='slats': mask=((y+x//4+phase)%pitch<max(2,pitch//2))
    elif kind=='leaves':
        field=rng.random((h,w))
        field=(field+np.roll(field,1,0)+np.roll(field,1,1)+np.roll(field,-1,1))*.25
        mask=field>.53
    else: raise ValueError(kind)
    # Two separated planes defeat endpoint agreement as an interval proof.
    depth=light['depth'].copy()
    for k,opaque in enumerate((mask,np.roll(mask,pitch//2,axis=1))):
        distance=float(rng.uniform(1.3+2*k,2.5+2*k))
        z=(12+.15)/(12-.15)-2*12*.15/((12-.15)*distance)
        depth=np.where(opaque,np.minimum(depth,z*.5+.5),depth)
    light['depth']=np.ascontiguousarray(depth,np.float32)
    return light


def distinct_projectors(fixture,count,steps=None):
    """Distinct transforms, blocker depths and settings in one additive draw set."""
    base=fixture[4];result=[]
    for i in range(count):
        offset=np.array([.11*(i-(count-1)*.5),.035*(i%3),.027*(i%2)])
        direction=V.S.unit(base['dir']+np.array([.025*(i%3-1),.01*(i%2),0.]))
        light=blockers(light_geometry(base,base['pos']+offset,direction),BLOCKERS[i%4],600+i)
        result.append(dict(light=light,uniforms=dict(projvol_noise_strength=(0.,.12,.126,.5)[i%4],
            projvol_noise_scale=(1.2,1.200001,.8,2.)[i%4],projvol_shadow_samples=1+i%4),
            **({'steps':steps} if steps is not None else {})))
    return result


def energy_metric(actual,reference,clear):
    lit=clear[:,:,:3].max(2)>1e-4
    delta=np.max(np.abs(actual[:,:,:3]-reference[:,:,:3]),2)/np.maximum(clear[:,:,:3].max(2),1e-4)
    return dict(error=float(delta[lit].max()) if lit.any() else float('inf'),
        lit=int(lit.sum()),reference_lit=int(np.sum(reference[:,:,:3].max(2)>1e-4)),
        pixels_over_bound=int(np.sum(lit & (delta>.05))))


def fine_walks(sweep,runner,base):
    """17 points per boundary, including the review's +/-1e-6 facing pair."""
    rows=[]
    fields={'noise':(.125,1e-6),'scale':(1.2,1e-6),'inactive_scale':(1.2,1e-6),
        'fog':(0.,1e-8),'budget':(4.,1e-5),'budget8':(8.,1e-5),'budget_zero':(0.,1e-7),
        'coverage':(.25,1e-6),'coverage_full':(1.,1e-6),'coverage_zero':(0.,1e-7),
        'frame':(48.,1e-5),'frame_zero':(0.,1e-7),'maximum_steps_zero':(0.,1e-7),
        'bigcap_min':(0.,1e-7),'bigcap_max':(1.,1e-7),
        'facing':(.95,1e-6),'inside':(0.,1e-6),'margin':(.05,1e-6),
        'distance_min':(1/6,1e-7),'distance_max':(.25,1e-7),
        'light_position':(0.,1e-6),'light_angle':(0.,1e-7),'height':(1080.,1e-4),
        'fine_medium':(.5,1e-6),'maximum':(100.,1e-5),'saturation_cost':(100.,1e-5)}
    for steps in (16,24,32,41,47):
        for kind in BLOCKERS:
            c=copy.deepcopy(base);c.update(size=[129,73],pose='inside',cones=1,resolution=steps,
                blocker=kind,geometry_seed=0,noise=0.,scale=1.2,fog=0.,taps=2,
                air=0,soft_filter=True,cookie='none',dither=0,maximum=100.,direct_budget=4.9)
            f=sweep.fixture(c);cam,light=f[0],f[4];axis=light['dir'];side=V.S.unit(np.cross(axis,[0.,0.,1.]))
            reference_cache={}
            for field,(center,epsilon) in fields.items():
                if field=='saturation_cost':
                    u=sweep.spec(c)['uniforms'];position=(cam.view@np.array([*light['pos'],1.]))[:3]
                    gain=u['godray_multiplier']*u['projvol_density']*(1+u['projvol_noise_strength'])*max(u['color'])*max(1.,u['projvol_shadow_tint'])
                    lo,hi=0.,1000.
                    for _ in range(32):
                        mid=(lo+hi)*.5
                        if V.host_request(sweep.host,'N',[*position,12.,u['projvol_g'],gain,mid,*np.linalg.inv(cam.proj).T.ravel()])[0]:hi=mid
                        else:lo=mid
                    center=hi;epsilon=max(hi,1.)*1e-6
                images=[];refs=[];clear=[];decisions=[]
                for k in range(-8,9):
                    value=center+k*epsilon; cc=copy.deepcopy(c);sp=sweep.spec(cc);ff=f;n=4.9
                    if field in ('noise','fine_medium'):sp['uniforms']['projvol_noise_strength']=max(value,0.)
                    elif field in ('scale','inactive_scale'):
                        sp['uniforms'].update(projvol_noise_scale=value,projvol_noise_strength=0. if field=='inactive_scale' else .12)
                    elif field=='fog':sp['uniforms']['projvol_fog_strength']=max(value,0.)
                    elif field in ('budget','budget8','budget_zero'):n=max(value,0.)
                    elif field in ('maximum','saturation_cost'):sp['kwargs']['maximum']=value
                    elif field.startswith(('coverage','frame','bigcap')) or field in ('height','maximum_steps_zero'):
                        n=float(V.host_request(sweep.host,'P',[0,1,min(max(value,0.),1.) if field.startswith('coverage') else 1.,
                            value if field=='height' else 1080.,6,steps,max(value,0.) if field=='maximum_steps_zero' else 0.,
                            min(max(value,0.),1.) if field.startswith('bigcap') else .6,
                            max(value,0.) if field.startswith('frame') else 48.,6.])[0])
                    elif field.startswith('light_'):
                        pos=light['pos']+side*value if field=='light_position' else light['pos']
                        direction=V.S.unit(axis+side*value) if field=='light_angle' else axis
                        # Reproject the same blockers with the moving lamp.
                        moving=blockers(light_geometry(light,pos,direction),kind,600)
                        ff=(*f[:4],moving)
                    else:
                        eye=cam.eye.copy();forward=axis
                        if field=='facing':forward=axis*value+side*np.sqrt(1-value*value)
                        elif field in ('inside','margin'):
                            planes=light['planes'][:5];planes=planes/np.linalg.norm(planes[:,:3],axis=1)[:,None]
                            distances=planes@np.array([*eye,1.]);plane=planes[int(np.argmin(distances))]
                            eye-=plane[:3]*(float(distances.min())-value*(12. if field=='margin' else 1.))
                        else:eye=light['pos']+V.S.unit(cam.eye-light['pos'])*(12.*value)
                        camera=V.S.Camera(eye,eye+forward*3,cam.W,cam.H)
                        g=V.S.cast_primary(light['scene'],camera);_,d,norm=V.S.encode_gbuffer(camera,g)
                        ff=(camera,g,d,norm,light)
                    # Caps change only the retired request, so Full receives
                    # exactly identical inputs. Cache only that fixed geometry
                    # and an exact serialized spec; moving poses are rerendered.
                    key=json.dumps(sp,sort_keys=True) if ff is f else None
                    cached=reference_cache.get(key) if key is not None else None
                    if cached is None:
                        ref=V.render_look(runner,ff,sp,definition=1)[0]
                        unshadowed=V.render_look(runner,ff,sp,definition=1,shadows=False)[0]
                        if key is not None:reference_cache[key]=(ref,unshadowed)
                    else:ref,unshadowed=cached
                    im=V.render_look(runner,ff,sp,definition=0,budget_steps=n)[0]
                    decisions.append(copy.deepcopy(runner.last_soft_decision));images.append(im);refs.append(ref)
                    clear.append(unshadowed)
                mask=np.maximum.reduce([im[:,:,:3].max(2) for im in clear])>1e-4
                denom=np.maximum.reduce([im[:,:,:3].max(2) for im in clear]);denom=np.maximum(denom,1e-4)
                per_step=[float((np.abs(b[:,:,:3]-a[:,:,:3]).max(2)/denom)[mask].max()) for a,b in zip(images,images[1:])]
                changed=[V.shaft_changes(a,b)[0] for a,b in zip(images,images[1:])]
                same=all(a.tobytes()==b.tobytes() for a,b in zip(images,refs))
                rows.append(dict(field=field,steps=steps,blocker=kind,center=center,epsilon=epsilon,points=17,
                    maximum_step_energy=max(per_step),changed_over_5_percent=max(changed),
                    additional_change_vs_Full=0. if same else float('inf'),Full_byte_identical=same,
                    all_fallback=all(not d['budgeted'] for d in decisions),lit=int(mask.sum())))
            print('fine_walk',steps,kind,'max_step',max(r['maximum_step_energy'] for r in rows if r['steps']==steps and r['blocker']==kind),flush=True)
    return rows


def mixed_checks(sweep,runner,base):
    rows=[]
    for count in (1,3,6,10):
        c=copy.deepcopy(base);c.update(size=[257,145],pose='inside',resolution=24,cones=1,blocker='none')
        f=sweep.fixture(c);sp=sweep.spec(c);ps=distinct_projectors(f,count)
        ref=V.render_look(runner,f,sp,definition=1,projectors=ps)[0]
        clear=V.render_look(runner,f,sp,definition=1,projectors=ps,shadows=False)[0]
        for mode in (0,2,3):
            im=V.render_look(runner,f,sp,definition=mode,budget_steps=4.9,projectors=ps)[0]
            rows.append(dict(cones=count,mode=mode,energy=energy_metric(im,ref,clear),
                identical=im.tobytes()==ref.tobytes(),dispatch=copy.deepcopy(runner.last_projector_dispatch),
                transforms=[dict(position=p['light']['pos'].tolist(),axis=p['light']['dir'].tolist(),uniforms=p['uniforms']) for p in ps]))
    return rows


def stale_variant_checks(sweep,runner,base):
    """Force stale BUDGET=1 permutations past CPU admission, in memory only."""
    rows=[]
    for steps in (4,8,16,24,32,41,47,55,64):
        c=copy.deepcopy(base);c.update(size=[129,73],pose='inside',resolution=steps,cones=1,
            blocker='fence',noise=0.,scale=1.2,fog=0.,air=0,soft_filter=True,cookie='none')
        f=sweep.fixture(c);sp=sweep.spec(c)
        ref=V.render_look(runner,f,sp,budget_steps=0.)[1]
        was=runner.cpu_fallback
        try:
            runner.cpu_fallback=False
            stale=V.render_look(runner,f,sp,budget_steps=4.9)[1]
        finally:runner.cpu_fallback=was
        rows.append(dict(steps=steps,identical=ref.tobytes()==stale.tobytes()))
    return rows


def discrete_controls(sweep,runner,base):
    """Integer/bool controls have no mathematical fine step; compare each
    side with Full, retaining Full's intended discrete change."""
    cases=[('authored',dict(steps=n),{}) for n in (4,8,16,24,31,32,33,41,47,55,64)]
    cases += [('dither',dict(dither=n),{}) for n in (0,1,2)]
    cases += [('shadow_taps',{},dict(projvol_shadow_samples=n)) for n in (1,2,3,4)]
    cases += [(key,{key:value},{}) for key in ('soft','air','equi','footprint','conservative') for value in (0,1)]
    cases += [('cookie',dict(cookie=value),{}) for value in (False,'checker')]
    cases += [('frustum',{},dict(projvol_frustum_clip=n)) for n in (0,1)]
    rows=[]
    for kind in BLOCKERS:
        c=copy.deepcopy(base);c.update(size=[129,73],pose='inside',resolution=32,cones=1,blocker=kind,
            noise=0.,scale=1.2,fog=0.,air=0,soft_filter=True,cookie='none',dither=0)
        f=sweep.fixture(c)
        for field,kw,u in cases:
            sp=sweep.spec(c);sp['kwargs'].update(kw);sp['uniforms'].update(u)
            ref=V.render_look(runner,f,sp,definition=1)[0]
            for mode in (0,3):
                im=V.render_look(runner,f,sp,definition=mode,budget_steps=4.9)[0]
                rows.append(dict(blocker=kind,field=field,kwargs=kw,uniforms=u,mode=mode,
                    identical=im.tobytes()==ref.tobytes(),reference_lit=int(np.sum(ref[:,:,:3].max(2)>1e-4))))
    return rows


def negative_controls(sweep,runner,base):
    """Each mutation must fail its named visual predicate, independently."""
    c=copy.deepcopy(base);c.update(size=[257,145],pose='inside',resolution=32,cones=1,
        blocker='fence',geometry_seed=0,noise=0.,scale=1.2,fog=0.,taps=2,air=0,
        soft_filter=True,cookie='none',dither=0,shadow_jitter=0,maximum=100.,direct_budget=4.9)
    f=sweep.fixture(c);sp=sweep.spec(c)
    ref=V.render_look(runner,f,sp,definition=1)[0]
    clear=V.render_look(runner,f,sp,definition=1,shadows=False)[0]
    # Restore the reviewed unsafe estimator and its actual CPU admission rule.
    old=V.Runner(V.G.SourceTree('342a2514642'),sweep.gl)
    folder=sweep.output/'reviewed_dispatch';folder.mkdir(exist_ok=True)
    old_host=V.build_scissor_host(folder,None,ref='342a2514642');old.host=old_host
    try:
        cam=f[0];axis=f[4]['dir'];side=V.S.unit(np.cross(axis,[0.,0.,1.]))
        images=[];decisions=[]
        for cosine in (.950001,.949999):
            direction=axis*cosine+side*np.sqrt(1-cosine*cosine)
            camera=V.S.Camera(cam.eye,cam.eye+direction*3,cam.W,cam.H)
            g=V.S.cast_primary(f[4]['scene'],camera);_,d,n=V.S.encode_gbuffer(camera,g)
            images.append(V.render_look(old,(camera,g,d,n,f[4]),sp,budget_steps=4.9)[0])
            decisions.append(copy.deepcopy(old.last_soft_decision))
        changed,lit=V.shaft_changes(*images)
        pop=dict(bug='admission_pop',predicate='changed/lit > 0.001',changed=changed,lit=lit,
            decisions=decisions,rejected=bool(lit and changed/lit>.001))
        missing=V.render_look(old,f,sp,budget_steps=4.9)[0]
        metric=energy_metric(missing,ref,clear)
        clear_missing=V.render_look(old,f,sp,budget_steps=4.9,shadows=False)[0]
        clear_metric=energy_metric(clear_missing,clear,clear)
        V.sheet(sweep.output/'fence_control.png',[
            ('Full / 32 authored samples',ref),('reviewed sparse estimator',missing),('unshadowed energy reference',clear)])
        V.sheet(sweep.output/'admission_pop_control.png',[
            ('reviewed facing 0.950001',images[0]),('reviewed facing 0.949999',images[1])])
        detail=dict(bug='missing_between_node_detail',predicate='shadowed error/unshadowed Full > 0.05 AND clear error <= 0.05',
            metric=metric,unshadowed_control=clear_metric,decision=copy.deepcopy(old.last_soft_decision),
            rejected=bool(metric['lit'] and metric['reference_lit'] and metric['error']>.05 and clear_metric['error']<=.05))
        return [pop,detail]
    finally:
        old_host.stdin.close();assert old_host.wait(timeout=5)==0


def shipped_costs(sweep,runner,repeats,paired_cost):
    rows=[]
    defaults=V.settings_defaults()
    looks=[('default',{k:v[1] for k,v in defaults.items()})]+[(l['id'],V.look_state(l,defaults)) for l in V.parse_looks()['looks']]
    rig=V.LookRig(sweep.gl,sweep.host)
    # Every shipped look retains its real controls; default/Cinematic get 4K
    # 1/3/6/10 brackets. All other looks get distinct six-cone 720p checks.
    for name,state in looks:
        sp=V.look_spec(state)
        for count in ((1,3,6,10) if name in ('default','b:cinematic') else (6,)):
            sp['post_controls']={n:state[V.PV+n][0] for n in ('Temporal','TemporalBlend','TemporalReject')}
            size=(4096,2160) if name in ('default','b:cinematic') else (1280,720)
            c=dict(size=size,pose='inside',blocker='none')
            f=sweep.fixture(c);ps=distinct_projectors(f,count)
            # Cost probes preserve each look's noise/taps/etc as shipped.
            for p in ps:p['uniforms']={}
            active=rig.runners(sp['dust'])['after']
            reference=V.render_look(active,f,sp,definition=1,projectors=ps)[0]
            clear=V.render_look(active,f,sp,definition=1,projectors=ps,shadows=False)[0]
            identity={}
            for label,mode in (('Auto',0),('Fast',3)):
                actual=V.render_look(active,f,sp,definition=mode,budget_steps=4.9,projectors=ps)[0]
                identity[label]=dict(identical=actual.tobytes()==reference.tobytes(),
                    **energy_metric(actual,reference,clear))
            measurements=paired_cost(dict(Full=active,Auto=active,Fast=active),f,sp,
                dict(Full=0.,Auto=4.9,Fast=3.185),False,count,repeats,post=True,projectors=ps,definitions=dict(Full=1,Auto=0,Fast=3))
            row=dict(look=name,cones=count,size=size,controls=sp,timing=measurements,identity=identity)
            rows.append(row);print('shipped_cost',row,flush=True)
    return rows
