"""WP-A paired 4K diagnostics, independent of the unchanged absolute cost gate.

Run --mode relative before edits, --mode ablate for one-fix-at-a-time removals,
and --mode optimized after edits. --mode options measures an OUTPUT-CHANGING
counter-stream candidate in memory and runs the numerical quality predicates.
All timestamps and moving-block bootstrap intervals are retained. No viewer
build, source mutation, commit, or HEAD movement. Explicit child watchdog.
"""
import os
os.environ['PYTHONUTF8'] = '1'
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts/code_tools'))
import argparse
import ctypes as C
import hashlib
import json
import re
import subprocess
import numpy as np
import sensor_iso_gl_check as S


def git_source(ref):
    return subprocess.check_output(['git', 'show', ref+':'+S.SHADER.relative_to(S.ROOT).as_posix()],
                                   cwd=S.ROOT, text=True, encoding='utf-8', timeout=15)


def replace(source, old, new):
    assert old in source, old
    return source.replace(old, new)


def body(source, signature):
    start = source.index(signature)
    end = source.index('\n}', start)+2
    return source[start:end]


def ablations(current, old):
    hashed = current
    for signature in ('uint sensorKey(', 'float sensorUniform(', 'vec2 sensorNormalPair('):
        hashed = replace(hashed, body(hashed, signature), body(old, signature))
    greens = replace(current, '''companion = fullResolution.y * sqrt(greenLuma*greenLuma +
        p.controls.y*p.controls.y*(greenChromaU*greenChromaU+greenChromaV*greenChromaV));''',
                     'companion = fullResolution.y;')
    greens = replace(greens,
                     'shotResidual = p.workingToCamera * sensorGaussian(c, pixel, p, 0x5a07u,greenShotDifference);',
                     'shotResidual = p.workingToCamera * sensorGaussian(c, pixel, p, 0x5a07u,greenShotDifference);\n            greenShotDifference *= sqrt(mu.y*0.5);')
    greens = replace(greens, 'companion = fullResolution.y * p.controls.w;',
                     'companion = fullResolution.y * p.physics.y * 0.707106781186548;')
    tail = replace(current, body(current, 'vec4 sensorPoissonQuantile('),
                   'vec4 sensorPoissonQuantile(vec4 lambda, vec4 u) { return sensorPoissonCDF(lambda,u); }')
    lattice = replace(current, 'vec2 position = vec2(pixel) * 0.5 + origin;',
                      'vec2 position = vec2(pixel) * 0.5;')
    return dict(hash=hashed, per_site_greens=greens, poisson_tail=tail, lattice=lattice)


def gpu():
    return subprocess.run(['nvidia-smi','--query-gpu=utilization.gpu,clocks.sm,power.draw',
                           '--format=csv,noheader'],capture_output=True,text=True,timeout=15).stdout.strip()


def counter_option(source):
    return replace(source,body(source,'uint sensorKey('),'''uint sensorKey(ivec2 pixel, SensorParameters p, uint domain)
{
    // DIAGNOSTIC ONLY: different random streams, full avalanche after each
    // counter increment. This is deliberately absent from production.
    uint state = sensorHash(uint(pixel.x)*0x9e3779b9u ^
        uint(pixel.y)*0x85ebca6bu ^ sensorContext(p));
    return sensorHash(state + domain*0x9e3779b9u);
}''')


def measure(r, m, sources, cases, count):
    caches = {name: {} for name in sources}
    rows = []
    names = list(sources)
    for iso, fixture in cases:
        u,p = m.get(0,iso)
        gradient = (.00001,1.) if fixture == 'gradient' else (1.,0.)
        light = 1. if fixture == 'gradient' else float(fixture)*p['gN']/p['K']
        samples = {name: [] for name in names}
        for index in range(count+64):
            u[S.CLOCK:S.CLOCK+8].view(np.uint32)[:] = (index,0)
            # Rotate and reverse order: each variant occupies every position.
            shift = index % len(names)
            order = names[shift:]+names[:shift]
            if (index//len(names))%2:
                order = order[::-1]
            for name in order:
                r.sensor, r.programs = sources[name], caches[name]
                r.prepare(u, light, w=3840, h=2160, cost=True, gradient=gradient)
                query = C.c_uint()
                r.gl.gen_queries(1,C.byref(query))
                r.gl.begin_query(0x88BF,query)
                r.gl.draw(0x0004,0,6)
                r.gl.end_query(0x88BF)
                ns = C.c_uint64()
                r.gl.query_u64(query,0x8866,C.byref(ns))
                r.delete_query(1,C.byref(query))
                if index >= 64:
                    samples[name].append(ns.value*1e-6)
        a = np.asarray(samples[names[0]])
        rng = np.random.default_rng(0x51c057)
        starts = rng.integers(0,count,size=(2000,count//16))
        indices = ((starts[:,:,None]+np.arange(16))%count).reshape(2000,-1)
        stats = {}
        for name in names:
            b = np.asarray(samples[name])
            ratio = b/a
            # Median paired ratios and moving-block bootstrap preserve pairing.
            stats[name] = dict(p50=float(np.median(b)),p95=float(np.percentile(b,95)),
                p95_ci=np.percentile(np.percentile(b[indices],95,axis=1),[2.5,97.5]).tolist(),
                paired_ratio=float(np.median(ratio)),
                paired_ratio_ci=np.percentile(np.median(ratio[indices],axis=1),[2.5,97.5]).tolist(),
                paired_delta_p50=float(np.median(b-a)))
            stats[name]['absolute_gate'] = 'PASS' if stats[name]['p95_ci'][1] <= .25 else 'FAIL'
        row = dict(iso=iso,fixture=fixture,n=count,stats=stats,samples=samples)
        rows.append(row)
        print(json.dumps({k:v for k,v in row.items() if k!='samples'}),flush=True)
    return rows


def worker(args):
    preopt = git_source('3efef2be20af4a3098c21e7d3b22cd54ea6c457c')
    (S.OUT/'cost_preopt.glsl').write_text(preopt,encoding='utf-8')
    old = git_source('8781da62617')
    old = re.sub(r'    mat3 readCholesky;[^\n]*\n','',old)
    current = S.SHADER.read_text(encoding='utf-8')
    sources = dict(baseline_8781da62617=old,current=preopt)
    if args.mode == 'ablate':
        sources = dict(current=preopt,**ablations(preopt,old))
    elif args.mode == 'optimized':
        sources['optimized'] = current
    elif args.mode == 'options':
        sources = dict(optimized=current,counter_streams=counter_option(current))
    cases = [(100,'gradient'),(25600,'gradient'),(25600,'0.5'),(25600,'5'),(25600,'19.9'),(25600,'20')]
    before = gpu()
    print('External GPU load before context: '+before,flush=True)
    m,r = S.Model(S.OUT/'native_probe'),S.Runner()
    result = dict(renderer=r.gl.renderer,head=subprocess.check_output(['git','rev-parse','HEAD'],text=True,timeout=10).strip(),
                  gpu_before=before,source_hashes={k:hashlib.sha256(v.encode()).hexdigest() for k,v in sources.items()},
                  provenance=S.provenance(current),
                  tester_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    result['cases'] = measure(r,m,sources,cases,args.pairs)
    result['gpu_after'] = gpu()
    if args.mode == 'options':
        # No source edits and no acceptance claims for this changed output.
        r.sensor,r.programs = sources['counter_streams'],{}
        r.identity_sensor = None
        suite = S.Suite(r,m)
        names = ('poisson','quantisation','ptc','brightness','gain_switch','clip','colour','determinism',
                 'black','adversarial','temporal','green_sites','green_peak','poisson_tail','hue_ramp','lattice_phase','hires')
        for name in names:
            suite.current_group = name
            S.progress('OUTPUT-CHANGING option quality: '+name)
            first = len(suite.checks)
            getattr(suite,name)()
            suite.groups[name] = 'PASS' if all(row['passed'] for row in suite.checks[first:]) else 'FAIL'
        result['quality'] = dict(groups=suite.groups,checks=suite.checks,tables=suite.tables,failures=suite.failures)
    (S.OUT/('cost_'+args.mode+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--worker',action='store_true')
    parser.add_argument('--mode',choices=('relative','ablate','optimized','options'),default='relative')
    parser.add_argument('--pairs',type=int,default=1024)
    parser.add_argument('--timeout',type=int,default=900)
    args = parser.parse_args()
    if args.worker:
        worker(args)
    else:
        raise SystemExit(subprocess.run([sys.executable,__file__,'--worker','--mode',args.mode,
                                        '--pairs',str(args.pairs)],timeout=args.timeout).returncode)
