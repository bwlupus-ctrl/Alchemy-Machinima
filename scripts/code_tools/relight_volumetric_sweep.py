"""Seeded production-WGL FIX5 gate (no viewer build, mutations only in memory).

Run python scripts/code_tools/relight_volumetric_sweep.py. The default 256
configurations use small odd/even targets; cost probes use 720p and 4K.
Reports retain every configuration and the worst configuration per metric.
"""
import argparse
import copy
import json
from functools import lru_cache
from pathlib import Path
import time

import numpy as np
import relight_volumetric_gl_check as V
from relight_volumetric_perf import ORDER_NOISE_MS, ORDER_NOISE_FRACTION, HARD_COST_TOLERANCE, hard_cost_noise

SEEDS = (0x1430716, 0x8550843, 0x8080979, 0x41004755,
         0x4F4F0505, 0x25005000, 0x64100361, 0x60001000)
REVIEWED = '76cd5cc2412'
CONTINUITY_BOUND = .001
ENERGY_BOUND = .05


class MemoryTree(V.G.SourceTree):
    def __init__(self, source):
        super().__init__('WORKTREE')
        self.source = source

    def read(self, path):
        return self.source if path.endswith(V.SHAFT) else super().read(path)


def block(source, marker):
    a = source.index(marker)
    b = source.index('{', a) + 1
    depth = 1
    while depth:
        depth += (source[b] == '{') - (source[b] == '}')
        b += 1
    return a, b


def configurations():
    result = []
    for seed in SEEDS:
        rng = np.random.default_rng(seed)
        for k in range(32):
            result.append(dict(seed=seed, index=k,
                size=[int(rng.choice((256, 257, 320, 321))), int(rng.choice((144, 145, 180, 181)))],
                pose=str(rng.choice(('backlit', 'inside', 'outside', 'toward_apex'))),
                fog=float(rng.choice((0., 1e-8, 1e-6, .01, .25, .5, 1.))),
                noise=float(rng.choice((0., .12, .124, .126, .25, .49, .5, .51, 1., rng.random()))),
                scale=float(rng.choice((.1, 1.2, 2., 8., rng.uniform(.1, 8.)))),
                time=float(rng.choice((0., 27., rng.uniform(0, 120)))),
                wind=rng.uniform(-1, 1, 3).tolist(),
                cookie=str(rng.choice(('none', 'checker', 'gobo'))),
                cones=int(rng.integers(1, 11)), overlap=float(rng.uniform(.01, 1)),
                resolution=int(rng.choice((4, 8, 16, 24, 32, 41, 47, 55, 64))),
                max_steps=float(rng.choice((0., 4., 4.9, 8.05, 12., 32.))),
                big_cap=float(rng.choice((0., .6, 1.))),
                coverage=float(rng.choice((.24999, .25, .25001, .99999, 1., rng.random()))),
                virtual_height=int(rng.choice((360, 720, 1080, 2160))),
                frame_budget=float(rng.choice((0., 4., 16., 48., 96.))),
                air=int(rng.integers(2)), soft_filter=bool(rng.integers(2)),
                mode=int(rng.choice((0, 2, 3))), hard=bool(rng.integers(2))))
    # Reproductions are part of the sweep, in addition to the random population.
    for kind in ('cookie', 'fog', 'weak', 'duplicate', 'interpolant'):
        c = copy.deepcopy(result[0])
        c.update(seed=0, index=kind, size=[640, 360], pose='backlit', fog=0.,
                 noise=.25 if kind in ('weak', 'duplicate') else 0., scale=1.2,
                 time=27., wind=[.37, .21, .19], cookie='checker' if kind == 'cookie' else 'none',
                 cones=1, resolution=64, mode=0, air=0, soft_filter=True,
                 hard=kind in ('weak', 'duplicate', 'interpolant'),
                 direct_budget=4.895 if kind == 'cookie' else 8.05)
        result.append(c)
    for kind in ('detail_cost', 'eight_cost', 'soft_energy', 'missing_soft', 'no_filter', 'shadow_jitter', 'saturated'):
        c = copy.deepcopy(result[0])
        c.update(seed=0,index=kind,size=[640,360],pose='inside',fog=0.,noise=.25,
                 scale=1.2,time=27.,wind=[.37,.21,.19],cookie='none',cones=6,
                 resolution=32,mode=0,hard=False,air=0,soft_filter=True,direct_budget=4.9)
        if kind == 'detail_cost': c['air'] = 1
        if kind == 'eight_cost': c.update(resolution=8,cones=1,direct_budget=1.)
        if kind == 'soft_energy': c.update(resolution=64,noise=1.,scale=2.,cones=1)
        if kind == 'missing_soft': c.update(noise=.12,resolution=64,direct_budget=8.)
        if kind == 'saturated': c.update(noise=.12,resolution=64,direct_budget=8.,maximum=.001,taps=2)
        if kind in ('no_filter','shadow_jitter'):
            c.update(noise=.12,resolution=32,direct_budget=8.,soft_filter=kind != 'no_filter',
                     shadow_jitter=int(kind == 'shadow_jitter'))
        result.append(c)
    # Explicitly exercise admitted work: a mostly-fallback random population
    # alone cannot establish the optimized kernel's accuracy.
    for steps in (32,41,47,55,64):
        for noise,scale in ((0.,.1),(.12,1.2),(.125,1.2)):
            c = copy.deepcopy(next(c for c in result if c['index'] == 'missing_soft'))
            c.update(index=f'admitted/{steps}/{noise}',resolution=steps,noise=noise,scale=scale,
                     direct_budget=4.9,cones=1)
            result.append(c)
    for steps in (32,64):
        for noise in (0.,.12,.125):
            for dither in (0,1):
                c=copy.deepcopy(result[-1])
                c.update(index=f'cinematic/{steps}/{noise}/{dither}',resolution=steps,noise=noise,
                         look='b:cinematic',dither=dither,taps=2,scale=1.2)
                result.append(c)
    for seed in SEEDS:
        rng=np.random.default_rng(seed)
        for k in range(8):
            c=copy.deepcopy(next(c for c in result if c['index']=='missing_soft'))
            c.update(seed=seed,index=f'admitted_random/{k}',cones=1,resolution=int(rng.choice((32,41,47,55,64))),
                     size=[int(rng.choice((256,257,320,321))),int(rng.choice((144,145,180,181)))],
                     noise=float(rng.choice((.12,.125,rng.uniform(0,.125)))),scale=float(rng.uniform(.1,1.2)),
                     time=float(rng.uniform(0,3600)),wind=rng.uniform(-1,1,3).tolist(),
                     dither=int(rng.integers(2)),direct_budget=float(rng.uniform(1,8)),taps=int(rng.integers(1,3)))
            if k%2: c['look']='b:cinematic'
            result.append(c)
    return result


@lru_cache(maxsize=1)
def base_spec():
    look = next(l for l in V.parse_looks()['looks'] if l['id'] == 'b:silhouette')
    return V.look_spec(V.look_state(look, V.settings_defaults()))


def spec_for(c):
    spec = (V.look_spec(V.look_state(next(l for l in V.parse_looks()['looks'] if l['id']==c['look']),V.settings_defaults()))
            if 'look' in c else copy.deepcopy(base_spec()))
    spec['kwargs'].update(steps=c['resolution'], rim=0., maximum=c.get('maximum',100.), half=True,
                          equi=0, air=c.get('air',0), soft=c.get('soft_filter',True), footprint=0, dither=c.get('dither',0),
                          cookie=False if c['cookie'] == 'none' else c['cookie'])
    spec['uniforms'].update(projvol_noise_strength=c['noise'], projvol_noise_scale=c['scale'],
        projvol_time=c['time'], projvol_wind=tuple(c['wind']), projvol_fog_strength=c['fog'],
        projvol_fog_ground_density=8., projvol_fog_falloff=.15, projvol_fog_base=.7,
        projvol_shadow_tint=0., projvol_shadow_samples=c.get('taps',1))
    spec['uniforms']['projvol_shadow_jitter_tap'] = c.get('shadow_jitter',0)
    return spec


def allocation(host, c, overlap_delta=0.):
    if 'direct_budget' in c:
        return c['direct_budget']
    total = c['coverage']*(1+(c['cones']-1)*(c['overlap']+overlap_delta))
    return float(V.host_request(host, 'P', [c['mode'], 1, c['coverage'], c['virtual_height']//2,
        6, c['resolution'], c['max_steps'], c['big_cap'], c['frame_budget'], total])[0])


class Sweep:
    def __init__(self, gl, host):
        self.gl, self.host, self.fixtures = gl, host, {}

    def fixture(self, c):
        key = (*c['size'], c['pose'],tuple(c.get('eye',())),tuple(c.get('target',())))
        if key not in self.fixtures:
            w, h = c['size']
            # Reuse the expensive analytic light-space depth map across sizes.
            group = next((v for k, v in self.fixtures.items() if k[2] == c['pose']), None)
            if group is not None:
                cam = group[0]
                camera = V.S.Camera(c.get('eye',cam.eye), c.get('target',cam.eye+cam.f), w, h)
                g = V.S.cast_primary(group[4]['scene'], camera)
                _, d, n = V.S.encode_gbuffer(camera, g)
                self.fixtures[key] = (camera, g, d, n, group[4])
            else:
                self.fixtures[key] = (V.backlit_fixture(w, h) if c['pose'] == 'backlit'
                                      else V.fixtures(w, h)[c['pose']])
        return self.fixtures[key]

    def fallback_identity(self, runner, c):
        sp, f = spec_for(c), self.fixture(c)
        full, raw, _ = V.render_look(runner,f,sp,layers=c['cones'])
        termination = runner.termination.tobytes()
        actual, capped, _ = V.render_look(runner,f,sp,budget_steps=allocation(self.host,c),layers=c['cones'])
        decision = copy.deepcopy(runner.last_soft_decision)
        return dict(decision=decision,identical=(full.tobytes(),raw.tobytes(),termination) ==
                    (actual.tobytes(),capped.tobytes(),runner.termination.tobytes()))

    def continuity(self, runner, c):
        spec = spec_for(c)
        f, n = self.fixture(c), allocation(self.host, c)
        a = V.render_look(runner, f, spec, layers=c['cones'], budget_steps=n, hard_beam=c['hard'])[0]
        results = {}
        for field in ('budget', 'fog', 'noise', 'overlap'):
            sp = copy.deepcopy(spec)
            budget = n
            if field == 'budget': budget += .01
            elif field == 'fog': sp['uniforms']['projvol_fog_strength'] += 1e-8
            elif field == 'noise': sp['uniforms']['projvol_noise_strength'] = min(c['noise']+1e-6, 1.)
            else: budget = allocation(self.host, c, 1e-4)
            b = V.render_look(runner, f, sp, layers=c['cones'], budget_steps=budget, hard_beam=c['hard'])[0]
            changed, lit = V.shaft_changes(a, b)
            results[field] = dict(fraction=changed/lit if lit else 1., changed=changed, lit=lit,
                                  valid_coverage=lit > 0)
        return results

    def energy(self, runner, c, hard=True):
        sp, f = spec_for(c), self.fixture(c)
        # Hold authored resolution at the 64-step energy reference for both
        # images. Definition must match Full at identical authored settings;
        # hard_identity separately proves every randomized resolution (8..64).
        sp['kwargs']['steps'] = 64
        ref = V.render_look(runner, f, sp, steps=64, hard_beam=hard, layers=c['cones'])[0]
        clear = V.render_look(runner, f, sp, steps=64, hard_beam=hard, shadows=False, layers=c['cones'])[0]
        im = V.render_look(runner, f, sp, budget_steps=allocation(self.host, c), hard_beam=hard, layers=c['cones'])[0]
        decision = copy.deepcopy(runner.last_soft_decision)
        lit = clear[:, :, :3].max(2) > 1e-4
        delta = np.max(np.abs(im[:, :, :3]-ref[:, :, :3]), 2)/np.maximum(clear[:, :, :3].max(2), 1e-4)
        reference_lit = int(np.sum(ref[:,:,:3].max(2) > 1e-4))
        return dict(error=float(delta[lit].max()) if lit.any() else float('inf'), lit=int(lit.sum()),
                    reference_lit=reference_lit, actual_lit=int(np.sum(im[:,:,:3].max(2)>1e-4)),
                    decision=decision, pixels_over_bound=int(np.sum(lit & (delta > ENERGY_BOUND))))

    def hard_identity(self, runner, c):
        sp, f = spec_for(c), self.fixture(c)
        full, raw, _ = V.render_look(runner, f, sp, hard_beam=True, layers=c['cones'])
        termination = runner.termination.tobytes()
        actual, capped, _ = V.render_look(runner, f, sp, budget_steps=allocation(self.host,c),
                                        hard_beam=True, layers=c['cones'])
        return (full.tobytes(),raw.tobytes(),termination) == (actual.tobytes(),capped.tobytes(),runner.termination.tobytes())


def timed(runner, fixture, spec, n, hard, layers, repeats, post=False):
    samples, stages = [], []
    warmups = 8 if post else 5
    history = np.zeros((fixture[0].H//2,fixture[0].W//2,4),np.float32) if post else None
    for k in range(warmups+repeats):
        _, _, stats = V.render_look(runner, fixture, spec, budget_steps=n, hard_beam=hard, layers=layers)
        if post:
            history = runner.temporal_resolve(fixture,history,fixture[0],k>0,blend=.86,reject=3.)
            stats['temporal_ms'] = runner.temporal_ms
            runner.bloom_feed(fixture,1.85,1.,stats)
        if k >= warmups:
            samples.append(sum(stats.values())); stages.append(stats)
    return dict(median_ms=float(np.median(samples)), p10_p90_ms=np.percentile(samples, [10, 90]).tolist(),
                stages_ms={key:float(np.median([r.get(key,0.) for r in stages])) for key in stages[0]})


def paired_cost(runners, fixture, spec, budgets, hard, layers, repeats, post=False):
    """Rotate identical warmed draws to prevent whole-block clock drift."""
    samples = {k:[] for k in runners}; stages = {k:[] for k in runners}
    histories = {k:np.zeros((fixture[0].H//2,fixture[0].W//2,4),np.float32) for k in runners}
    names = list(runners)
    for i in range(8+max(repeats,15)):
        for j in range(len(names)):
            k = names[(i+j)%len(names)]; runner = runners[k]
            _,_,stats = V.render_look(runner,fixture,spec,budget_steps=budgets[k],hard_beam=hard,layers=layers)
            if post:
                histories[k] = runner.temporal_resolve(fixture,histories[k],fixture[0],i>0,blend=.86,reject=3.)
                stats['temporal_ms'] = runner.temporal_ms; runner.bloom_feed(fixture,1.85,1.,stats)
            if i >= 8: samples[k].append(sum(stats.values())); stages[k].append(stats)
    return {k:dict(median_ms=float(np.median(samples[k])),p10_p90_ms=np.percentile(samples[k],[10,90]).tolist(),
                   stages_ms={s:float(np.median([r.get(s,0.) for r in stages[k]])) for s in stages[k][0]}) for k in names}


def fallback_boundaries(sweep, runner):
    base = copy.deepcopy(next(c for c in configurations() if c['index'] == 'missing_soft'))
    base.update(size=[321,181],cones=1,resolution=32,taps=2)
    fixture = sweep.fixture(base); cam,light = fixture[0],fixture[4]
    axis = light['dir']; side = V.S.unit(np.cross(axis,[0.,0.,1.]))
    pairs = []
    # Execute the production no-early-saturation proof at its actual switch.
    # A factor-two envelope margin leaves both rendered estimates unclamped.
    spec=spec_for(base);u=spec['uniforms'];center=(cam.view@np.array([*light['pos'],1.]))[:3]
    gain=u['godray_multiplier']*u['projvol_density']*(1+u['projvol_noise_strength'])*max(u['color'])*max(1.,u['projvol_shadow_tint'])
    lo,hi=0.,1000.
    for _ in range(32):
        mid=(lo+hi)*.5
        allowed=V.host_request(sweep.host,'N',[*center,12.,u['projvol_g'],gain,mid,*np.linalg.inv(cam.proj).T.ravel()])[0]
        if allowed: hi=mid
        else: lo=mid
    a,b=copy.deepcopy(base),copy.deepcopy(base)
    a['maximum'],b['maximum']=lo*(1-1e-5),hi*(1+1e-5)
    pairs.append(('saturation_cost',a,b))
    for field,center,epsilon in (('noise',.125,1e-6),('scale',1.2,1e-6),('fog',0.,1e-8)):
        a,b = copy.deepcopy(base),copy.deepcopy(base)
        a[field],b[field] = max(center-epsilon,0.),center+epsilon
        pairs.append((field,a,b))
    for field in ('facing','inside','inside_edge','distance','range'):
        a,b = copy.deepcopy(base),copy.deepcopy(base)
        for c,sign in ((a,-1),(b,1)):
            eye = cam.eye.copy(); forward = axis
            if field == 'facing':
                cosine = .95+sign*1e-6
                forward = axis*cosine+side*np.sqrt(1-cosine*cosine)
            elif field in ('inside','inside_edge'):
                planes=light['planes'][:5]; planes=planes/np.linalg.norm(planes[:,:3],axis=1)[:,None]
                distances=planes@np.array([*eye,1.]); p=planes[int(np.argmin(distances))]
                eye -= p[:3]*(float(distances.min())-(.6 if field == 'inside' else 0.)+sign*1e-6)
            else:
                eye = light['pos']+V.S.unit(cam.eye-light['pos'])*((3. if field == 'range' else 2.)+sign*1e-6)
            c['eye']=eye.tolist(); c['target']=(eye+forward*3).tolist()
        pairs.append((field,a,b))
    rows = []
    for name,a,b in pairs:
        images=[]; decisions=[]; energy=[]
        for c in (a,b):
            images.append(V.render_look(runner,sweep.fixture(c),spec_for(c),budget_steps=allocation(sweep.host,c))[0])
            decisions.append(copy.deepcopy(runner.last_soft_decision));energy.append(sweep.energy(runner,c,hard=False))
        changed,lit=V.shaft_changes(*images)
        rows.append(dict(boundary=name,configurations=[a,b],decisions=decisions,energy=energy,
                         changed=changed,lit=lit,fraction=changed/max(lit,1)))
    return rows


def review_costs(sweep, runner, repeats):
    old = V.Runner(V.G.SourceTree('462f5fac75c'),sweep.gl); old.host=sweep.host
    rows=[];failures=[]
    for steps in (4,8,16,32):
        for cap in range(1,9):
            c=copy.deepcopy(next(c for c in configurations() if c['index']=='detail_cost'))
            c.update(size=[1280,720],resolution=steps,air=int(cap%2),soft_filter=bool(cap%3),direct_budget=float(cap))
            sp=spec_for(c); fixture=sweep.fixture(c)
            timing=paired_cost(dict(Full=runner,Soft=runner),fixture,sp,dict(Full=0.,Soft=float(cap)),False,6,repeats)
            identity=sweep.fallback_identity(runner,c)
            full,soft=(timing[k]['median_ms'] for k in ('Full','Soft'))
            row=dict(config=c,timing=timing,ratio=soft/full,**identity)
            if soft > full+max(ORDER_NOISE_MS,full*ORDER_NOISE_FRACTION): failures.append(row)
            if not identity['decision']['budgeted'] and not identity['identical']: failures.append(row)
            rows.append(row);print('cap_cost',row,flush=True)
    # Calibrate the accepted kernel as well as fallback identity. Exercise PCF
    # on/off, all supported shadow counts, zero/low noise and static dither.
    for noise in (0.,.12):
        for filtering,taps in ((False,1),(True,1),(True,2),(True,4)):
            c=copy.deepcopy(next(c for c in configurations() if c['index']=='missing_soft'))
            c.update(size=[1280,720],resolution=32,cones=6,noise=noise,soft_filter=filtering)
            sp=spec_for(c);sp['kwargs']['dither']=1;sp['uniforms']['projvol_shadow_samples']=taps
            timing=paired_cost(dict(Full=runner,Soft=runner),sweep.fixture(c),sp,dict(Full=0.,Soft=4.9),False,6,repeats)
            full,soft=(timing[k]['median_ms'] for k in ('Full','Soft'))
            row=dict(calibration=True,config=c,render_controls=sp,timing=timing,ratio=soft/full)
            if soft > full+max(ORDER_NOISE_MS,full*ORDER_NOISE_FRACTION):failures.append(row)
            rows.append(row);print('admitted_cost',row,flush=True)
    for kind,steps,air,cap,reported in (('detail_cost',32,1,4.9,[1.847,2.722]),('eight_cost',8,0,1.,[.647,.797])):
        c=copy.deepcopy(next(c for c in configurations() if c['index']==kind))
        c.update(size=[1280,720],cones=6,resolution=steps,air=air,direct_budget=cap)
        sp=spec_for(c)
        timing=paired_cost(dict(Full=runner,Before=old,After=runner),sweep.fixture(c),sp,
                           dict(Full=0.,Before=cap,After=cap),False,6,repeats)
        full,after=(timing[k]['median_ms'] for k in ('Full','After'))
        row=dict(repro=kind,config=c,timing=timing,ratio=after/full,review_full_soft_ms=reported,
                 **sweep.fallback_identity(runner,c))
        if after > full+max(ORDER_NOISE_MS,full*ORDER_NOISE_FRACTION) or not row['identical']:failures.append(row)
        rows.append(row);print('review_repro',row,flush=True)
    return rows,failures


def cost_checks(sweep, runner, repeats):
    rows, failures = [], []
    for size in ((1280, 720), (4096, 2160)):
        for noise in (.12, .25):
            for cones in (1, 3, 6, 10):
                c = configurations()[0]
                c.update(size=list(size), pose='inside', noise=noise, fog=0., scale=1.2,
                         time=27., wind=[.37, .21, .19], cookie='none', cones=cones,
                         resolution=32, max_steps=0., coverage=1., overlap=1.,
                         virtual_height=size[1], frame_budget=48., big_cap=.6)
                cinematic = next(l for l in V.parse_looks()['looks'] if l['id'] == 'b:cinematic')
                sp = V.look_spec(V.look_state(cinematic,V.settings_defaults()))
                sp['kwargs'].update(steps=32,soft=True,rim=0.,maximum=100.)
                sp['uniforms'].update(projvol_noise_strength=noise,projvol_noise_scale=1.2,
                    projvol_time=27.,projvol_wind=(.37,.21,.19),projvol_shadow_samples=2)
                for hard in (False, True):
                    group = {}
                    budgets = {name:0. if mode == 1 else allocation(sweep.host,dict(c,mode=mode))
                               for mode,name in ((1,'Full'),(0,'Auto'),(3,'Fast'))}
                    measured = paired_cost(dict(Full=runner,Auto=runner,Fast=runner),sweep.fixture(c),sp,
                                           budgets,hard,cones,repeats,post=True)
                    for mode, name in ((1, 'Full'), (0, 'Auto'), (3, 'Fast')):
                        c['mode'] = mode
                        n = budgets[name]
                        t = measured[name]
                        row = dict(size=list(size), noise=noise, cones=cones, hard=hard, mode=name, steps=n,
                                   config=copy.deepcopy(c), render_controls=copy.deepcopy(sp), **t)
                        rows.append(row); group[name] = t['median_ms']
                        print('cost', row, flush=True)
                    allowance = max(ORDER_NOISE_MS, group['Auto']*ORDER_NOISE_FRACTION)
                    if hard:
                        if any(abs(group[m]-group['Full']) > hard_cost_noise(group['Full']) for m in ('Auto','Fast')):
                            failures.append(dict(rule='Hard_equals_Full',config=copy.deepcopy(c),hard=hard,
                                                 costs=group,noise_ms=hard_cost_noise(group['Full'])))
                    elif group['Fast'] > group['Auto']+allowance or group['Auto'] > group['Full']+max(ORDER_NOISE_MS, group['Full']*ORDER_NOISE_FRACTION):
                        failures.append(dict(config=copy.deepcopy(c), hard=hard, costs=group, noise_ms=allowance))
                    if not hard and size == (4096, 2160) and cones == 6 and noise == .12:
                        for mode in ('Auto', 'Fast'):
                            if group[mode] > .60*group['Full']:
                                failures.append(dict(rule='six_cone_saving', mode=mode,
                                    config=copy.deepcopy(c), hard=hard, costs=group, maximum_ratio=.60))
    return rows, failures


def identity(gl, host, source):
    current = V.Runner(MemoryTree(source), gl); current.host = host
    original = V.Runner(V.G.SourceTree('13028c968b4'), gl); original.host = host
    c = configurations()[0]
    c.update(size=[321, 181], pose='toward_apex', cookie='checker', noise=.25, fog=.1)
    sweep = Sweep(gl, host)
    sp = spec_for(c); sp['kwargs'].update(equi=1, air=1, footprint=1)
    rows = []
    for n in (40, 41, 47, 48, 55, 64):
        for half in (False, True):
            a, ar, _ = V.render_look(original, sweep.fixture(c), sp, steps=n, half=half)
            at = original.termination.copy() if half else None
            b, br, _ = V.render_look(current, sweep.fixture(c), sp, steps=n, half=half)
            same = ar.tobytes() == br.tobytes() and (not half or at.tobytes() == current.termination.tobytes())
            rows.append(dict(steps=n, half=half, identical=same, maximum_error=float(np.max(np.abs(a-b)))))
    return rows


def mutations(source):
    old = V.G.SourceTree(REVIEWED).read(V.G.SHADERS+'/'+V.SHAFT)
    soft_marker = '        if (budgeted && projvol_hard_beam_shadows == 0)\n        {'
    a, b = block(source, soft_marker)
    x = old.index('        if (budgeted && !fine_detail && projvol_hard_beam_shadows == 0 && step_count > 4.0)')
    _, y = block(old, '        if (budgeted && projvol_hard_beam_shadows == 0 && (fine_detail || medium_detail > 0.0 || step_count < 5.0))')
    soft = old[x:y]
    # Restore the unsafe dispatch as well as the historical integration bug;
    # production CPU admission otherwise makes these bad kernels unreachable.
    yield 'cookie', (source[:a]+'        float medium_detail = 0.0;\n'+soft+source[b:]).replace('!fine_detail && projvol_hard_beam_shadows == 0)', '!fine_detail && false && projvol_hard_beam_shadows == 0)')
    yield 'fog', (source[:a]+'        float medium_detail = projvol_fog_strength > 0.0 ? 1.0 : 0.0;\n'+soft+source[b:]).replace('!fine_detail && projvol_hard_beam_shadows == 0)', '!fine_detail && false && projvol_hard_beam_shadows == 0)')
    yield 'missing_soft', source.replace('    frag_color = vec4(shaft,',
        '    if (budgeted) shaft = vec3(0.0);\n    frag_color = vec4(shaft,')
    # Reintroduce the forbidden capped Hard path along with each historical
    # Hard bug. Otherwise the production Full-only guard makes it unreachable.
    source = source.replace('''#if defined(PROJVOL_HARD_BEAM) && PROJVOL_HARD_BEAM
#undef PROJVOL_BUDGET_ENABLE
#define PROJVOL_BUDGET_ENABLE 0
#endif
''', '')
    marker = '        // One fixed lattice jointly integrates visibility, cookie, local medium'
    a = source.index(marker); b = source.index('        // The common uniform-distance composite', a)
    hard = source[a:b]
    # Execute the same local estimates in two weighted loops; only the work
    # duplication is restored, with visibility/cookie/analytic no longer shared.
    twice = hard
    start = twice.index('        for (int j =')
    loop = twice[start:].replace('vec3 value = fine_dt*', 'vec3 value = 0.5*fine_dt*')
    yield 'duplicate', source[:a]+twice[:start]+loop+loop+source[b:]
    weak = hard.replace('        for (int j =', '''        vec2 weak_medium = vec2(
            projvolMedium(world_origin+world_rate*(t0+0.25*march_len)),
            projvolMedium(world_origin+world_rate*(t0+0.75*march_len)));
        for (int j =''', 1)
    weak = weak.replace('float medium = projvolMedium((projvol_inv_modelview*vec4(d*ft,1.0)).xyz);', '''float medium = mix(mix(weak_medium.x,weak_medium.y,
                clamp(2.0*(float(j)+roffset)/float(godray_res)-0.5,0.0,1.0)),
                projvolMedium((projvol_inv_modelview*vec4(d*ft,1.0)).xyz),smoothstep(0.125,0.5,projvol_noise_strength));''')
    yield 'weak', source[:a]+weak+source[b:]
    x = old.index('                float analytic = 0.0;', old.index('if (medium_detail < 1.0)'))
    y = old.index('                float visibility =', x)
    interpolation = hard.replace('        for (int j =', '''        int cached_segment = -1;
        vec3 radiance_nodes = vec3(0.0);
        float radiance_start = t0, radiance_length = dt;
        for (int j =''', 1)
    interpolation = interpolation.replace('            float analytic = projvolAnalytic(d,ft,fp);', old[x:y])
    yield 'interpolant', source[:a]+interpolation+source[b:]
    yield 'endpoint', source.replace('budgeted ? start == 0.0 : i == 0', 'start == 0.0').replace(
        'budgeted ? endpoint == 1.0 : i == shaft_steps-1', 'endpoint == 1.0')


def controls(sweep, runner, source, repeats):
    results = []
    focused = {c['index']: c for c in configurations() if c['seed'] == 0}
    for kind, shader in mutations(source):
        bad = V.Runner(MemoryTree(shader), sweep.gl); bad.host = sweep.host
        if kind in ('cookie', 'fog'):
            bad.cpu_fallback = False  # restore the historical unsafe CPU dispatch too
            c = focused[kind]
            metric = sweep.continuity(bad, c)
            rejected = any(m['fraction'] > CONTINUITY_BOUND for m in metric.values())
        elif kind in ('weak','missing_soft'):
            c = focused[kind]
            metric = sweep.energy(bad, c, hard=kind == 'weak')
            rejected = metric['lit'] > 0 and metric['reference_lit'] > 0 and metric['error'] > ENERGY_BOUND
            if kind == 'missing_soft':
                metric['continuity'] = sweep.continuity(bad,c)
                metric['continuity_coverage_rejected'] = all(
                    not m['valid_coverage'] and m['fraction'] > CONTINUITY_BOUND for m in metric['continuity'].values())
                rejected = rejected and metric['continuity_coverage_rejected']
        elif kind == 'endpoint':
            metric = identity(sweep.gl, sweep.host, shader)
            rejected = any(not m['identical'] for m in metric)
            c = dict(steps=[41, 47, 55], integration='equiangular')
        else:
            c = copy.deepcopy(focused[kind])
            c.update(size=[1280, 720], pose='inside', cones=6 if kind == 'duplicate' else 7,
                     noise=.25 if kind == 'duplicate' else 0., resolution=32)
            c.pop('direct_budget')
            sp = spec_for(c)
            metric = paired_cost(dict(Full=bad,Auto=bad,Fast=bad,unmutated_Auto=runner,unmutated_Fast=runner),
                sweep.fixture(c),sp,dict(Full=0.,Auto=6.857,Fast=4.457,unmutated_Auto=6.857,unmutated_Fast=4.457),
                True,c['cones'],max(repeats,31))
            auto, fast, full = (metric[m]['median_ms'] for m in ('Auto', 'Fast', 'Full'))
            if kind == 'interpolant':
                rejected = fast > auto+max(ORDER_NOISE_MS, auto*ORDER_NOISE_FRACTION)
            else:
                baseline = metric['unmutated_Auto']
                rejected = auto > max(full, baseline['median_ms'])+max(ORDER_NOISE_MS, baseline['median_ms']*ORDER_NOISE_FRACTION)
            metric['Hard_Full_byte_identity'] = sweep.hard_identity(bad,c)
            metric['timing_predicate_failed'] = bool(rejected)
        row = dict(bug=kind, rejected=bool(rejected), config=c, metric=metric)
        results.append(row); print('negative_control', row, flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=V.G.REPO/'.tmp/shaft_perf/fix5_sweep')
    parser.add_argument('--repeats', type=int, default=31)
    parser.add_argument('--visual-only', action='store_true', help='diagnostic; omits required cost/control gates')
    args = parser.parse_args(); args.repeats = max(args.repeats, 15)
    args.out = args.out.resolve(); args.out.mkdir(parents=True, exist_ok=True)
    gl = V.G.GL(); assert 'RTX 5090' in gl.renderer, gl.renderer
    host = V.build_scissor_host(args.out, None)
    source = V.G.SourceTree('WORKTREE').read(V.G.SHADERS+'/'+V.SHAFT)
    runner = V.Runner(MemoryTree(source), gl); runner.host = host
    sweep = Sweep(gl, host)
    report = dict(renderer=gl.renderer, seeds=list(SEEDS), visual_look='b:silhouette', visual_base=base_spec(), cost_look='b:cinematic', cost_stages='march, upsample, temporal, bloom', continuity_bound=CONTINUITY_BOUND,
                  energy_bound=ENERGY_BOUND, configurations=[], worst={}, checks=[])
    start = time.monotonic()
    try:
        for i, c in enumerate(configurations()):
            continuity, energy = sweep.continuity(runner, c), sweep.energy(runner, c)
            soft_energy = sweep.energy(runner, c, hard=False)
            row = dict(config=c, budget=allocation(host, c), continuity=continuity, energy=energy,
                       soft_energy=soft_energy, hard_full_identical=sweep.hard_identity(runner,c),
                       soft_identity=sweep.fallback_identity(runner,c))
            report['configurations'].append(row)
            for key, value in [(k, m['fraction']) for k, m in continuity.items()]+[('energy', energy['error']),('soft_energy',soft_energy['error'])]:
                if key not in report['worst'] or value > report['worst'][key]['value']:
                    report['worst'][key] = dict(value=value, config=c, budget=row['budget'])
            if i % 16 == 0: print('sweep', i+1, 'worst', {k: v['value'] for k, v in report['worst'].items()}, flush=True)
        report['identity'] = identity(gl, host, source)
        old = V.Runner(V.G.SourceTree('462f5fac75c'),gl); old.host=host
        c = next(c for c in configurations() if c['index']=='soft_energy')
        report['review_energy'] = dict(config=c,before=sweep.energy(old,c,hard=False),after=sweep.energy(runner,c,hard=False))
        report['checks'].append(dict(name='review_22_percent_repro',passed=report['review_energy']['before']['error'] > ENERGY_BOUND and
            report['review_energy']['after']['error'] <= ENERGY_BOUND and report['review_energy']['after']['reference_lit'] > 0))
        report['boundaries'] = fallback_boundaries(sweep,runner)
        report['checks'].append(dict(name='fallback_boundaries',passed=all(
            r['lit'] > 0 and r['fraction'] == 0. and all(
                e['lit'] > 0 and e['reference_lit'] > 0 and e['error'] <= ENERGY_BOUND for e in r['energy'])
            for r in report['boundaries']) and all(any(d['budgeted'] for d in r['decisions']) and
                not all(d['budgeted'] for d in r['decisions']) for r in report['boundaries'] if r['boundary'] != 'inside_edge')))
        report['checks'].append(dict(name='randomized_continuity', passed=all(
            m['fraction'] == 0. for r in report['configurations'] for m in r['continuity'].values())))
        report['checks'].append(dict(name='hard_energy', passed=all(r['energy']['error'] <= ENERGY_BOUND for r in report['configurations'])))
        report['checks'].append(dict(name='soft_energy', passed=all(r['soft_energy']['error'] <= ENERGY_BOUND for r in report['configurations'])))
        report['checks'].append(dict(name='nonempty_reference_coverage', passed=all(
            r[key]['lit'] > 0 and r[key]['reference_lit'] > 0 for r in report['configurations'] for key in ('energy','soft_energy')) and all(
            m['lit'] > 0 for r in report['configurations'] for m in r['continuity'].values())))
        report['checks'].append(dict(name='Hard_Full_byte_identity',passed=all(r['hard_full_identical'] for r in report['configurations'])))
        report['checks'].append(dict(name='Soft_fallback_Full_byte_identity',passed=all(
            r['soft_identity']['decision']['budgeted'] or r['soft_identity']['identical'] for r in report['configurations'])))
        report['checks'].append(dict(name='Full_odd_even_byte_identity', passed=all(r['identical'] for r in report['identity'])))
        report['checks'].append(dict(name='admitted_Soft_exercised',passed=sum(
            r['soft_energy']['decision']['budgeted'] for r in report['configurations']) >= 15))
        if not args.visual_only:
            report['costs'], failures = cost_checks(sweep, runner, args.repeats)
            report['review_costs'], extra_failures = review_costs(sweep,runner,args.repeats)
            failures += extra_failures
            report['worst']['Soft_Full_cost_ratio'] = max(
                (dict(value=r['ratio'],config=r) for r in report['review_costs']),key=lambda r:r['value'])
            for row in report['costs']:
                if row['mode'] == 'Full': continue
                other = next(m for m in report['costs'] if all(m[k] == row[k] for k in ('size','noise','cones','hard')) and m['mode'] == ('Auto' if row['mode'] == 'Fast' else 'Full'))
                key = row['mode']+'_cost_ratio'
                ratio = row['median_ms']/other['median_ms']
                if key not in report['worst'] or ratio > report['worst'][key]['value']:
                    report['worst'][key] = dict(value=ratio, config=row, reference=other)
            report['checks'].append(dict(name='cost_ordering', passed=not failures, failures=failures,
                                         hard_equality_fraction=HARD_COST_TOLERANCE,hard_equality_floor_ms=ORDER_NOISE_MS))
            report['controls'] = controls(sweep, runner, source, args.repeats)
            report['checks'].append(dict(name='seven_mutations_rejected', passed=all(r['rejected'] for r in report['controls'])))
        report['complete_gate'] = not args.visual_only
        report['passed'] = all(c['passed'] for c in report['checks']) and report['complete_gate']
    finally:
        host.stdin.close(); assert host.wait(timeout=5) == 0
        report['seconds'] = time.monotonic()-start
        (args.out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(checks=report['checks'], worst=report['worst'], seconds=report['seconds']), indent=2), flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
