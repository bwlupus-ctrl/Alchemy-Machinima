"""Seeded production-WGL FIX3 gate (no viewer build, mutations only in memory).

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
                resolution=int(rng.choice((8, 16, 24, 32, 41, 47, 55, 64))),
                max_steps=float(rng.choice((0., 4., 4.9, 8.05, 12., 32.))),
                big_cap=float(rng.choice((0., .6, 1.))),
                coverage=float(rng.choice((.24999, .25, .25001, .99999, 1., rng.random()))),
                virtual_height=int(rng.choice((360, 720, 1080, 2160))),
                frame_budget=float(rng.choice((0., 4., 16., 48., 96.))),
                mode=int(rng.choice((0, 2, 3))), hard=bool(rng.integers(2))))
    # Reproductions are part of the sweep, in addition to the random population.
    for kind in ('cookie', 'fog', 'weak', 'duplicate', 'interpolant'):
        c = copy.deepcopy(result[0])
        c.update(seed=0, index=kind, size=[640, 360], pose='backlit', fog=0.,
                 noise=.25 if kind in ('weak', 'duplicate') else 0., scale=1.2,
                 time=27., wind=[.37, .21, .19], cookie='checker' if kind == 'cookie' else 'none',
                 cones=1, resolution=64, mode=0, hard=kind in ('weak', 'duplicate', 'interpolant'),
                 direct_budget=4.895 if kind == 'cookie' else 8.05)
        result.append(c)
    return result


@lru_cache(maxsize=1)
def base_spec():
    look = next(l for l in V.parse_looks()['looks'] if l['id'] == 'b:silhouette')
    return V.look_spec(V.look_state(look, V.settings_defaults()))


def spec_for(c):
    spec = copy.deepcopy(base_spec())
    spec['kwargs'].update(steps=c['resolution'], rim=0., maximum=100., half=True,
                          equi=0, air=0, footprint=0, dither=0,
                          cookie=False if c['cookie'] == 'none' else c['cookie'])
    spec['uniforms'].update(projvol_noise_strength=c['noise'], projvol_noise_scale=c['scale'],
        projvol_time=c['time'], projvol_wind=tuple(c['wind']), projvol_fog_strength=c['fog'],
        projvol_fog_ground_density=8., projvol_fog_falloff=.15, projvol_fog_base=.7,
        projvol_shadow_tint=0., projvol_shadow_samples=1)
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
        key = (*c['size'], c['pose'])
        if key not in self.fixtures:
            w, h = c['size']
            # Reuse the expensive analytic light-space depth map across sizes.
            group = next((v for k, v in self.fixtures.items() if k[2] == c['pose']), None)
            if group is not None:
                cam = group[0]
                camera = V.S.Camera(cam.eye, cam.eye+cam.f, w, h)
                g = V.S.cast_primary(group[4]['scene'], camera)
                _, d, n = V.S.encode_gbuffer(camera, g)
                self.fixtures[key] = (camera, g, d, n, group[4])
            else:
                self.fixtures[key] = (V.backlit_fixture(w, h) if c['pose'] == 'backlit'
                                      else V.fixtures(w, h)[c['pose']])
        return self.fixtures[key]

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
            results[field] = dict(fraction=changed/max(lit, 1), changed=changed, lit=lit)
        return results

    def energy(self, runner, c):
        sp, f = spec_for(c), self.fixture(c)
        # Hold authored resolution at the 64-step energy reference for both
        # images. Definition must match Full at identical authored settings;
        # hard_identity separately proves every randomized resolution (8..64).
        sp['kwargs']['steps'] = 64
        ref = V.render_look(runner, f, sp, steps=64, hard_beam=True, layers=c['cones'])[0]
        clear = V.render_look(runner, f, sp, steps=64, hard_beam=True, shadows=False, layers=c['cones'])[0]
        im = V.render_look(runner, f, sp, budget_steps=allocation(self.host, c), hard_beam=True, layers=c['cones'])[0]
        lit = clear[:, :, :3].max(2) > 1e-4
        delta = np.max(np.abs(im[:, :, :3]-ref[:, :, :3]), 2)/np.maximum(clear[:, :, :3].max(2), 1e-4)
        return dict(error=float(delta[lit].max()) if lit.any() else 0., lit=int(lit.sum()),
                    pixels_over_bound=int(np.sum(lit & (delta > ENERGY_BOUND))))

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
                    for mode, name in ((1, 'Full'), (0, 'Auto'), (3, 'Fast')):
                        c['mode'] = mode
                        n = 0. if mode == 1 else allocation(sweep.host, c)
                        t = timed(runner, sweep.fixture(c), sp, n, hard, cones, repeats, post=True)
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
    yield 'cookie', (source[:a]+'        float medium_detail = 0.0;\n'+soft+source[b:]).replace('!fine_detail && projvol_hard_beam_shadows == 0)', '!fine_detail && false && projvol_hard_beam_shadows == 0)')
    yield 'fog', (source[:a]+'        float medium_detail = projvol_fog_strength > 0.0 ? 1.0 : 0.0;\n'+soft+source[b:]).replace('!fine_detail && projvol_hard_beam_shadows == 0)', '!fine_detail && false && projvol_hard_beam_shadows == 0)')
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
            c = focused[kind]
            metric = sweep.continuity(bad, c)
            rejected = any(m['fraction'] > CONTINUITY_BOUND for m in metric.values())
        elif kind == 'weak':
            c = focused[kind]
            metric = sweep.energy(bad, c)
            rejected = metric['error'] > ENERGY_BOUND
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
            metric = {}
            for mode, name in ((1, 'Full'), (0, 'Auto'), (3, 'Fast')):
                n = 0. if mode == 1 else (6.857 if name == 'Auto' else 4.457)
                metric[name] = timed(bad, sweep.fixture(c), sp, n, True, c['cones'], max(repeats, 15))
            auto, fast, full = (metric[m]['median_ms'] for m in ('Auto', 'Fast', 'Full'))
            if kind == 'interpolant':
                metric['unmutated_Auto'] = timed(runner, sweep.fixture(c), sp, 6.857, True, c['cones'], max(repeats, 15))
                metric['unmutated_Fast'] = timed(runner, sweep.fixture(c), sp, 4.457, True, c['cones'], max(repeats, 15))
                rejected = fast > auto+max(ORDER_NOISE_MS, auto*ORDER_NOISE_FRACTION)
            else:
                baseline = timed(runner, sweep.fixture(c), sp, 6.857, True, c['cones'], max(repeats, 15))
                metric['unmutated_Auto'] = baseline
                rejected = auto > max(full, baseline['median_ms'])+max(ORDER_NOISE_MS, baseline['median_ms']*ORDER_NOISE_FRACTION)
            metric['Hard_Full_byte_identity'] = sweep.hard_identity(bad,c)
            rejected = rejected or not metric['Hard_Full_byte_identity']
        row = dict(bug=kind, rejected=bool(rejected), config=c, metric=metric)
        results.append(row); print('negative_control', row, flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=V.G.REPO/'.tmp/shaft_perf/fix3_sweep')
    parser.add_argument('--repeats', type=int, default=15)
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
            row = dict(config=c, budget=allocation(host, c), continuity=continuity, energy=energy,
                       hard_full_identical=sweep.hard_identity(runner,c))
            report['configurations'].append(row)
            for key, value in [(k, m['fraction']) for k, m in continuity.items()]+[('energy', energy['error'])]:
                if key not in report['worst'] or value > report['worst'][key]['value']:
                    report['worst'][key] = dict(value=value, config=c, budget=row['budget'])
            if i % 16 == 0: print('sweep', i+1, 'worst', {k: v['value'] for k, v in report['worst'].items()}, flush=True)
        report['identity'] = identity(gl, host, source)
        report['checks'].append(dict(name='randomized_continuity', passed=all(
            m['fraction'] <= CONTINUITY_BOUND for r in report['configurations'] for m in r['continuity'].values())))
        report['checks'].append(dict(name='hard_energy', passed=all(r['energy']['error'] <= ENERGY_BOUND for r in report['configurations'])))
        report['checks'].append(dict(name='Hard_Full_byte_identity',passed=all(r['hard_full_identical'] for r in report['configurations'])))
        report['checks'].append(dict(name='Full_odd_even_byte_identity', passed=all(r['identical'] for r in report['identity'])))
        if not args.visual_only:
            report['costs'], failures = cost_checks(sweep, runner, args.repeats)
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
            report['checks'].append(dict(name='six_mutations_rejected', passed=all(r['rejected'] for r in report['controls'])))
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
