"""Refresh distribution and 60-second moving-camera take on real WGL.

Production owners/planner/draw/GLSL execute; octree/VBO scene services are the
existing 256-AABB fixture. All frames remain in the raw record, including spikes.
No viewer build is implied. Never discard an outlier to meet the 1.5 ms gate.
"""
import os
os.environ['PYTHONUTF8'] = '1'
import argparse
import ctypes as C
import json
import time
from pathlib import Path
import numpy as np
from weather_cpu_check import ROOT

OUT = ROOT / '.tmp/weather_port/final_cost'


def distribution(values):
    return dict(zip(('p50','p95','p99','max'), map(float, np.percentile(values,[50,95,99,100]))))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference', default='WORKTREE')
    p.add_argument('--kind', choices=['cover','shelter'], required=True)
    p.add_argument('--label', required=True)
    p.add_argument('--unpaced', action='store_true')
    a = p.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if a.kind == 'cover':
        from weather_particles_gl_check import run
        c = run(a.reference, cost=False, profile_only=True, benchmark_context=True, context_key=a.label)
    else:
        from weather_shelter_gl_check import run
        c = run(a.reference, cost=True, benchmark_context=True, context_key=a.label)
    n, gl = c['native'], c['gl']
    c['set']('AdvancedShelterEnabled',int(a.kind == 'shelter'))
    c['set']('SnowEnabled',1)
    c['set']('PrecipVisibilityEnabled',1)
    c['set']('PersistentWetEnabled',1)
    c['set']('WindSource',1)
    c['set']('WindSpeed',7)
    c['set']('WindVariation',0)
    c['set']('WindGustiness',0)
    c['set']('ShelterUpdateHz',60)
    n.cover_prepare.argtypes = [C.c_double]*3+[C.c_int]
    n.cover_dirty.argtypes = [C.c_double]*3
    if a.kind == 'shelter':
        n.shelter_prepare.argtypes = [C.c_double]*4+[C.c_int]
    # Time is continuous after fixture setup; settle only outside the take.
    def work(tick, phase):
        seconds = 10000 + tick/60
        # 8 m/s crosses a world tile every four seconds and leaves every source
        # rectangle inside a realistic moving-camera traverse.
        x = tick*(8/60)
        if tick >= 2056: x += 4096
        if a.kind == 'shelter':
            n.shelter_scene(0,float(tick%17)*.01,4,1)
            c['step'](seconds)
            n.shelter_prepare(seconds,x,0,0,1)
        else:
            c['step'](seconds)
            n.cover_dirty(x,0,4)
            n.cover_prepare(x,0,0,0)
    for tick in range(256):
        work(tick,'warmup')
    gl.finish()
    count = 6000 if a.unpaced else 3600
    queries = (C.c_uint*count)()
    gl.gen_queries(count,queries)
    rows=[]
    started=time.perf_counter()
    for i in range(count):
        if not a.unpaced:
            remaining=started+i/60-time.perf_counter()
            if remaining>0: time.sleep(remaining)
        phase = 'tile_entry' if (256+i)//240 != (255+i)//240 else 'steady_moving'
        # Teleport is an explicit event, never a discarded steady-state spike.
        if i == 1800: phase='teleport'
        if a.kind == 'shelter': n.shelter_sync()
        else: n.fixture_sync()
        before=c['counts']().copy()
        gl.begin_query(0x88BF,queries[i])
        start=time.perf_counter()
        work(256+i,phase)
        cpu=(time.perf_counter()-start)*1000
        gl.end_query(0x88BF)
        # Submit each display frame, without a production wait or readback.
        gl.fn('glFlush',None)()
        after=c['counts']()
        rows.append(dict(frame=i,phase=phase,cpu_ms=cpu,traversals=int(after[0]-before[0])))
        if i%600==0: print(a.label,a.kind,i,'of',count,flush=True)
    gl.finish()
    for q,row in zip(queries,rows):
        value=C.c_uint64()
        gl.query_u64(q,0x8866,C.byref(value))
        row['gpu_ms']=value.value/1e6
        row['total_ms']=row['cpu_ms']+row['gpu_ms']
    gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(count,queries)
    steady=[r for r in rows if r['phase']!='teleport']
    spikes=[r for r in rows if r['total_ms']>1.5]
    result=dict(reference=a.reference,kind=a.kind,label=a.label,frames=count,device=gl.renderer,
                paced_hz=None if a.unpaced else 60,elapsed_seconds=time.perf_counter()-started,
                total_ms=distribution([r['total_ms'] for r in rows]),
                steady_ms=distribution([r['total_ms'] for r in steady]),
                spikes=spikes,spikes_count=len(spikes),spikes_per_take=None if a.unpaced else len(spikes),
                camera_speed_mps=8,teleport_frame=1800,warmup_calls=256,
                gate_passed=all(r['total_ms']<=1.5 for r in steady),rows=rows,
                enable_cpu_ms={k:v for k,v in c['result'].items() if k in ('cold_prepare_ms','reenable_prepare_ms')},
                scope='Production owner/planner/traversal/GLSL; 256-AABB scene services simulated; native viewer integration unmeasured.')
    (OUT/(a.label+'_'+a.kind+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ('rows','spikes')}),flush=True)
    print('SPIKES',json.dumps(spikes[:30]),flush=True)
    assert result['gate_passed'], 'steady-state refresh exceeds 1.5 ms'


if __name__ == '__main__':
    main()
