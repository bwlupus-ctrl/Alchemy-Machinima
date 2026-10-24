"""Performance gates driven by production C++ decisions and production WGL shaders.

Invoked by relight_volumetric_gl_check.py; no viewer build, no copied shader math.
"""
import numpy as np

BASE = '13028c968b4'


def performance_checks(V, args, gl, host, report):
    def check(name, passed, **data):
        report['checks'].append(dict(name='performance/'+name,passed=bool(passed),**data))
        print('performance/'+name,'PASS' if passed else 'FAIL',data,flush=True)

    def budget(mode, coverage, height, resolution, total, maximum=0., cap=.6, frame=48.):
        return float(V.host_request(host,'P',[mode,1,coverage,height,6,resolution,maximum,cap,frame,total])[0])

    def sphere(fixture, position=None):
        cam,_,_,_,light = fixture
        c = (cam.view@np.array([*(light['pos'] if position is None else position),1.]))[:3]
        w,h = cam.W//2,cam.H//2
        v = V.host_request(host,'A',[w,h,12,*c,*cam.proj.T.ravel()])
        return float(np.clip((v[3]-v[1])/w,0,1)*np.clip((v[4]-v[2])/h,0,1)) if v[0] else 1.

    scenes = V.fixtures(args.width,args.height)
    recovery=V.host_request(host,'V',[])
    shader_mgr=(V.G.REPO/'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    check('variant_failure_reload_recovery',recovery==[0.,0.,0.,1.,3.,1.] and 'gPipeline.projectorVolumetricShaderLoaded(success,' in shader_mgr,
          failed_failed_reload_class1_successful_reload_retry_level_recovered=recovery)
    rig = V.LookRig(gl,host)
    looks = V.parse_looks()['looks']; defaults = V.settings_defaults()
    specs = {look['id']:V.look_spec(V.look_state(look,defaults)) for look in looks}
    if args.performance_cost_only:
        # Exact named profile controls, on the same analytic inside-cone fixture
        # for both refs. Identical overlapping cones isolate repeated march cost.
        # Termination MRT, temporal, upsample and bloom are all included.
        spec = V.look_spec(V.look_state(next(l for l in looks if l['id']=='b:cinematic'),defaults))
        spec['kwargs'].update(steps=32,soft=True,rim=0.,maximum=100.)
        spec['uniforms']['projvol_shadow_samples']=2
        spec['bloom']=(1.85,1.)
        after = rig.runners(spec['dust'])['after']; before = V.Runner(V.G.SourceTree(BASE),gl)
        before.host = host
        f = scenes['inside']; cov = sphere(f)
        def timed(runner, mode, n, hard):
            steps = budget(mode,cov,args.height//2,32,n*cov)
            history = np.zeros((args.height//2,args.width//2,4),np.float32)
            runs=[]
            for k in range(8+max(args.repeats,15)):
                _,_,stats = V.render_look(runner,f,spec,layers=n,budget_steps=steps,hard_beam=hard)
                history=runner.temporal_resolve(f,history,f[0],k>0,blend=.86,reject=3.)
                stats['temporal_ms']=runner.temporal_ms
                runner.bloom_feed(f,1.85,1.,stats)
                if k>=8:runs.append(stats)
            totals=[sum(s.values()) for s in runs]
            return dict(steps_per_cone=steps if mode!=1 else 32.,
                        median_ms=float(np.median(totals)),p10_p90_ms=np.percentile(totals,[10,90]).tolist(),
                        stages_ms={k:float(np.median([s.get(k,0) for s in runs])) for k in runs[0]})
        for n in (1,3,6):
            original=timed(before,1,n,False)
            report['measurements'].append(dict(cones=n,mode='baseline',shadows='soft',**original))
            print('baseline',n,original,flush=True)
            for mode,name in ((1,'Full'),(0,'Auto'),(3,'Fast')):
                for hard in (False,True):
                    result=timed(after,mode,n,hard)
                    report['measurements'].append(dict(cones=n,mode=name,shadows='hard' if hard else 'soft',**result))
                    print(name,n,'hard' if hard else 'soft',result,flush=True)
        check('cost_bracket',len(report['measurements'])==21,size=[args.width,args.height],warmups=8,samples=max(args.repeats,15),
              controls=dict(Resolution=32,ShadowSamples=2,SoftProjectorShadows=True,BloomFeed=1.85,TemporalBlend=.86,TemporalReject=3.),
              fixture='Cinematic Haze medium, identical inside cones, no rim; GPU draw costs only')
        return

    # Full proves byte identity against the requested commit, including raw MRT,
    # full-res surface rims and bloom, not a same-source toggle comparison.
    baseline = {}
    back = V.backlit_fixture(args.width,args.height)
    full_fixtures=dict(scenes,backlit=back,grazing=V.grazing_fixture(scenes['inside'],0.))
    for look in looks:
        spec=specs[look['id']]; after=rig.runners(spec['dust'])['after']
        if spec['dust'] not in baseline:
            saved=V.G.PREAMBLE
            try:
                if spec['dust']:V.G.PREAMBLE+='\n#define PROJVOL_DUST_ENABLE 1\n'
                r=V.Runner(V.G.SourceTree(BASE),gl)
            finally:V.G.PREAMBLE=saved
            if spec['dust']:r.__class__=V.DustRunner;r.volume=rig.volume
            baseline[spec['dust']]=r
        before=baseline[spec['dust']]; before.host=host; failures=[]
        def bloom_image(r,f):
            r.bloom_feed(f,*spec['bloom'],{})
            result=np.empty((f[0].H,f[0].W,4),np.float32)
            gl.read(0,0,f[0].W,f[0].H,0x1908,0x1406,result.ctypes.data)
            return result
        for pose,f in full_fixtures.items():
            for half in (False,True):
                steps=V.production_steps(host,f,spec['resolution'],half)[0]
                a,ar,_=V.render_look(before,f,spec,steps=steps,half=half)
                term=before.termination.copy() if half else None
                ab=bloom_image(before,f) if half and spec['bloom'][0]>0 else None
                b,br,_=V.render_look(after,f,spec,steps=steps,half=half,budget_steps=budget(1,sphere(f),f[0].H//2,spec['resolution'],1.))
                same=a.tobytes()==b.tobytes() and ar.tobytes()==br.tobytes()
                if half:same &= term.tobytes()==after.termination.tobytes()
                if ab is not None:same &= ab.tobytes()==bloom_image(after,f).tobytes()
                if not same:failures.append(dict(pose=pose,half=half,max_error=float(np.max(np.abs(a-b)))))
        check(look['id']+'/Full_identity',not failures,fixtures=len(full_fixtures)*2,failures=failures)

    # Pure CPU decisions: never round a count; cap transitions and entrants must
    # be continuous; total allocations may not exceed the requested budget.
    for mode in (0,2,3):
        heights=np.linspace(179.,1081.,1501)
        counts=np.array([budget(mode,1.,h,32,6.) for h in heights])
        crossing=np.array([budget(mode,c,1080.,32,1.+c) for c in np.linspace(0.,1.,1001)])
        allocations=[budget(mode,c,1080.,32,sum(cs)) for cs in ([1.,.2,.001],[1.]*6) for c in cs]
        check(str(mode)+'/continuous_decisions',np.max(np.abs(np.diff(counts)))<.02 and
              np.max(np.abs(np.diff(crossing)))<.1 and sum(allocations[-6:])<=48.+1e-4,
              max_resize_delta=float(np.max(np.abs(np.diff(counts)))),max_coverage_delta=float(np.max(np.abs(np.diff(crossing)))),
              six_steps=allocations[-6:])
    for sp in (specs['b:clean'],specs['b:searchlight']):
        runner=rig.runners(sp['dust'])['after']
        commands=V.host_request(host,'Q',[0,0,16,32,0.])
        zero_steps=int(next(c[2] for c in commands if c[:2]==['i','godray_res']))
        zero=V.render_look(runner,scenes['inside'],sp,steps=zero_steps,budget_steps=0.,rim=0.)[1]
        check('zero_share/'+str(sp['kwargs']['equi']),np.count_nonzero(zero)==0,
              allocation=budget(0,0.,args.height//2,32,1.),nonzero_values=int(np.count_nonzero(zero)))

    # Searchlight keeps its authored angular integration advantage under every
    # budget. Same 1024-step reference and 1.5 ratio as the existing P6 gate.
    sp=specs['b:searchlight'];runner=rig.runners(False)['after'];f=scenes['toward_apex']
    ref=V.render_look(runner,f,sp,steps=1024,shadows=False,half=False,equi=0,air=0,footprint=0)[0]
    mask=ref[:,:,:3].max(2)>1e-5
    apex={}
    for mode in (0,2,3):
        n=budget(mode,sphere(f),f[0].H,sp['resolution'],sphere(f))
        err=[]
        for equi in (0,1):
            im=V.render_look(runner,f,sp,shadows=False,half=False,budget_steps=n,equi=equi,air=0,footprint=0)[0]
            err.append(float(np.sqrt(np.mean((im[:,:,:3]-ref[:,:,:3])[mask]**2))))
        apex[str(mode)]=dict(uniform_rmse=err[0],angular_rmse=err[1],ratio=err[0]/max(err[1],1e-20))
    check('Searchlight_apex',all(v['ratio']>=1.5 for v in apex.values()),required_ratio=1.5,modes=apex)

    sp=dict(specs['b:clean'],kwargs=dict(specs['b:clean']['kwargs'],maximum=.01,rim=0.))
    runner=rig.runners(False)['after'];raw=[]
    for count in (1.999999,2.000001):
        # Direct probe of shaft alpha: the half-res upsample deliberately writes
        # alpha=0, and Runner's raw result contains half-float bit patterns.
        bits=V.render_look(runner,scenes['inside'],sp,budget_steps=count,shadows=False,half=False,beam=1)[1]
        raw.append(bits.view(np.float16).astype(np.float32))
    lit=(raw[0][:,:,:3].max(2)>.009)&(raw[0][:,:,3]>1e-5)
    moment_change=float(np.max(np.abs(raw[1][:,:,3][lit]-raw[0][:,:,3][lit])/raw[0][:,:,3][lit])) if np.any(lit) else float('inf')
    check('saturated_beam_moment',lit.sum()>100 and moment_change<.001,
          max_relative_alpha_change=moment_change,bound=.001,probes=int(lit.sum()))

    def sequence(runner, frames, spec, counts, preroll=None, hard=False):
        images=[]; previous=frames[0][0]; history=np.zeros((previous.H//2,previous.W//2,4),np.float32);valid=False
        indices=([0]*preroll if preroll else [])+list(range(len(frames)))
        for k in indices:
            im,_,_=V.render_look(runner,frames[k],spec,budget_steps=counts[k],hard_beam=hard)
            if preroll is not None:
                history=runner.temporal_resolve(frames[k],history,previous,valid)
                previous=frames[k][0];valid=True;im=history.copy()
            images.append(im[:,:,:3].copy())
        return images[preroll or 0:]

    worst=0.
    for look in looks:
        spec=specs[look['id']]; runner=rig.runners(spec['dust'])['after']; results={}
        for pose in ('inside','toward_apex'):
            frames=V.micro_frames(scenes[pose])
            for mode in (0,2,3):
                counts=[budget(mode,sphere(f),f[0].H//2,spec['resolution'],sphere(f)) for f in frames]
                for hard in (False,True):
                    for pre,label in ((None,'raw'),(0,'cold'),(3,'warm')):
                        ims=sequence(runner,frames,spec,counts,pre,hard=hard)
                        pairs=V.motion_fractions(ims);results[f'{pose}/{mode}/{int(hard)}/{label}']=max(pairs)
                        worst=max(worst,max(pairs))
        check(look['id']+'/motion',max(results.values())<=V.MOTION_PIXEL_BOUND,bound=V.MOTION_PIXEL_BOUND,pairs_per_sequence=8,maxima=results)

    # Strong noise without the Dust permutation must retain fixed medium cells
    # too. This catches accidentally keying the stable path only off Dust.
    sp=dict(specs['b:dust_storm'],dust=False,uniforms=dict(specs['b:dust_storm']['uniforms'],projvol_dust=0))
    runner=rig.runners(False)['after'];results={}
    for pose in ('inside','toward_apex'):
        fs=V.micro_frames(scenes[pose])
        for mode in (0,2,3):
            counts=[budget(mode,sphere(f),f[0].H//2,sp['resolution'],6.*sphere(f)) for f in fs]
            for pre,label in ((None,'raw'),(0,'cold'),(3,'warm')):
                results[f'{pose}/{mode}/{label}']=max(V.motion_fractions(sequence(runner,fs,sp,counts,pre)))
    worst=max(worst,max(results.values()))
    check('strong_noise_without_dust',max(results.values())<=V.MOTION_PIXEL_BOUND,bound=V.MOTION_PIXEL_BOUND,maxima=results)

    # Explicit moving budget transitions, rim ON. Primary cone stays in view;
    # moving secondary influence spheres enter horizontally, cross the smooth
    # cap region, and share the budget. Sphere coordinates use the real cameras.
    spec=dict(specs['b:clean'],kwargs=dict(specs['b:clean']['kwargs'],rim=.5))
    runner=rig.runners(False)['after']; frames=V.micro_frames(scenes['inside'])
    transition={}
    for mode in (0,2,3):
        for case in ('integer','sub_one','low125','low150','low175','refine2','refine3','refine4','refine6','refine8','coverage','entry','overlap'):
            counts=[];entrants=[]
            for k,f in enumerate(frames):
                cam=f[0]; cov=sphere(f)
                # A 1 mm camera move changes a real projected influence bound.
                # The extra sphere is just touching a side of the viewport.
                # Left-most box vertex is x-r, at depth z+r. Equality
                # x-r=(z+r)*tan*fov_aspect is precisely the zero-area edge.
                pos=frames[0][0].eye+frames[0][0].f*35.-frames[0][0].r*((35.+12.)*cam.tan*cam.aspect+12.-.001*(k-4))
                entrant=sphere(f,pos)
                entrants.append(entrant)
                if case=='integer':n=7.+(k-4)*.00025
                elif case=='sub_one':n=1.+(k-4)*.00025
                elif case.startswith('low'):n=float(case[3:])/100.+(k-4)*.00025
                elif case=='refine2':n=2.+(k-4)*.00025
                elif case=='refine3':n=3.+(k-4)*.00025
                elif case=='refine4':n=4.+(k-4)*.00025
                elif case=='refine6':n=6.+(k-4)*.00025
                elif case=='refine8':n=8.+(k-4)*.00025
                elif case=='coverage':n=budget(mode,.25+(k-4)*.0001,cam.H//2,32,1.)
                elif case=='entry':n=budget(mode,cov,cam.H//2,32,cov+entrant,frame=4.)
                else:n=budget(mode,cov,cam.H//2,32,cov+2.*(.5+(k-4)*.0001),frame=8.)
                counts.append(n)
            if case=='entry':
                check(str(mode)+'/entry_fixture',entrants[0]==0. and entrants[-1]>0. and max(counts)>min(counts),
                      entrant_coverage=entrants,steps=counts)
            for equi in (0,1):
                sp=dict(spec,kwargs=dict(spec['kwargs'],equi=equi))
                for hard in (False,True):
                    maxima={label:max(V.motion_fractions(sequence(runner,frames,sp,counts,pre,hard=hard)))
                            for pre,label in ((None,'raw'),(0,'cold'),(3,'warm'))}
                    worst=max(worst,max(maxima.values()))
                    transition[f'{mode}/{case}/{equi}/{int(hard)}']=dict(steps=counts,**maxima)
    check('transitions_rim_on',all(max(v[k] for k in ('raw','cold','warm'))<=V.MOTION_PIXEL_BOUND for v in transition.values()),
          bound=V.MOTION_PIXEL_BOUND,transitions=transition)

    # Single-tap visibility needs internal antialiasing at whole-chord budgets.
    # Check the entire fixed-lattice interval and its geometric join.
    low={}
    for center in np.linspace(1.,4.,61):
        counts=[float(center)+(k-4)*.00025 for k in range(len(frames))]
        low[f'{center:.2f}']={label:max(V.motion_fractions(sequence(runner,frames,spec,counts,pre)))
                             for pre,label in ((None,'raw'),(0,'cold'),(3,'warm'))}
    low_max=max(max(v.values()) for v in low.values());worst=max(worst,low_max)
    check('low_count_shadow_filtering',low_max<=V.MOTION_PIXEL_BOUND,
          bound=V.MOTION_PIXEL_BOUND,centers=low)

    # A Hard shortcut must not silently bypass the global integration caps.
    for name,f in (('b:cinematic',scenes['inside']),('b:dust_storm',back)):
        sp=specs[name];runner=rig.runners(sp['dust'])['after']
        definition=[V.render_look(runner,f,sp,budget_steps=n,hard_beam=True)[0] for n in (12.,4.)]
        check('hard_caps_change_integration/'+name,definition[0].tobytes()!=definition[1].tobytes(),
              max_pixel_change=float(np.max(np.abs(definition[0]-definition[1]))))

    # Thin occluders: measure transported dark lane samples normalized by the
    # same camera's unobstructed beam. Bound compares Hard to today's Soft;
    # intentional shadow tint is disabled so light in a shadow is a real leak.
    spec=specs['b:silhouette'];runner=rig.runners(False)['after'];leaks={}
    for fraction in (1.,.3):
        thin=V.backlit_fixture(args.width,args.height,bar_radius=.015,blocker_fraction=fraction)
        for mode in (1,0,2,3):
            n=budget(mode,sphere(thin),thin[0].H//2,spec['resolution'],6.*sphere(thin))
            ref=V.render_look(runner,thin,spec,shadows=False,rim=0.)[0]
            soft=V.render_look(runner,thin,spec,soft=True,rim=0.)[0]
            hard=V.render_look(runner,thin,spec,budget_steps=n,hard_beam=True,soft=True,rim=0.)[0]
            full_hard=V.render_look(runner,thin,spec,hard_beam=True,soft=True,rim=0.)[0]
            band=V.LaneBand(thin,ref)
            ss=band.samples(soft);hs=band.samples(hard);rs=band.samples(ref)
            mask=band.valid & (rs>1e-5)&(ss<.95*rs)
            leakage=float(np.max(np.maximum(hs[mask]-ss[mask],0)/rs[mask])) if np.any(mask) else 1.
            hard_error=float(np.max(np.abs(hs[mask]-band.samples(full_hard)[mask])/rs[mask])) if np.any(mask) else 1.
            unshadowed_leak=float(np.max(np.maximum(rs[mask]-ss[mask],0)/rs[mask])) if np.any(mask) else 0.
            fs=V.micro_frames(thin)
            ims={label:sequence(runner,fs,spec,[n]*len(fs),pre,hard=True)
                 for pre,label in ((None,'raw'),(0,'cold'),(3,'warm'))}
            bands=[V.LaneBand(f,V.render_look(runner,f,spec,budget_steps=n,shadows=False,rim=0.)[0]) for f in fs]
            common=np.logical_and.reduce([b.cols for b in bands])
            maxima={}
            for label,images in ims.items():
                profiles=[b.column_mean(b.samples(im))[common]/np.maximum(b.reference[common],1e-9) for b,im in zip(bands,images)]
                maxima[label]=max(float(np.mean(np.abs(b-a)>.05*np.maximum(a,1e-4))) for a,b in zip(profiles,profiles[1:]))
            worst=max(worst,max(maxima.values()))
            leaks[f'{fraction}/{mode}']=dict(probes=int(mask.sum()),max_extra_leak=leakage,full_hard_error=hard_error,
                                  unshadowed_negative_control=unshadowed_leak,soft_contrast=band.contrast(soft),hard_contrast=band.contrast(hard),motion=maxima)
    check('thin_occluder',all(v['probes']>0 and v['max_extra_leak']<=.05 and v['full_hard_error']<=.05 and
          v['unshadowed_negative_control']>.05 and max(v['motion'].values())<=V.MOTION_PIXEL_BOUND for v in leaks.values()),
          max_extra_unobstructed_fraction=.05,reference='Full Soft, same look and camera',modes=leaks)
    # State what happens to the standard seven-lane fan, rather than relaxing
    # its existing contrast gate when Hard changes the appearance.
    spec=specs['b:silhouette'];ref=V.render_look(runner,back,spec,shadows=False,rim=0.)[0];band=V.LaneBand(back,ref)
    soft=V.render_look(runner,back,spec,soft=True,rim=0.)[0]
    hard=V.render_look(runner,back,spec,hard_beam=True,soft=True,rim=0.)[0]
    fs=V.micro_frames(back);bands=[V.LaneBand(f,V.render_look(runner,f,spec,shadows=False,rim=0.)[0]) for f in fs]
    common=np.logical_and.reduce([b.cols for b in bands]);fan_motion={}
    for pre,label in ((None,'raw'),(0,'cold'),(3,'warm')):
        images=sequence(runner,fs,spec,[0.]*len(fs),pre,hard=True)
        profiles=[b.column_mean(b.samples(im))[common]/np.maximum(b.reference[common],1e-9) for b,im in zip(bands,images)]
        fan_motion[label]=max(float(np.mean(np.abs(b-a)>.05*np.maximum(a,1e-4))) for a,b in zip(profiles,profiles[1:]))
    worst=max(worst,max(fan_motion.values()))
    check('hard_fan_lanes',band.lane_count(hard)>=3 and band.contrast(hard)>=V.LANE_CONTRAST_MIN and max(fan_motion.values())<=V.MOTION_PIXEL_BOUND,
          lanes=band.lane_count(hard),required_contrast=V.LANE_CONTRAST_MIN,motion=fan_motion,
          soft_contrast=band.contrast(soft),hard_contrast=band.contrast(hard),
          lane_contrast_change=band.contrast(hard)-band.contrast(soft))
    report['motion_margin']=V.MOTION_PIXEL_BOUND-worst
