"""Display-frame weather refresh limits, publication, mutation and paired cost.

Geometry/cull/sort services simulate 256 AABBs. Production scheduling, depth
transactions, vertex limiter and weather consumers execute on real WGL. No
viewer build, octree/VBO timing or exclusive GPU ownership is implied.
"""
import os
os.environ['PYTHONUTF8'] = '1'
import argparse
import ctypes as C
import json
import subprocess
import time
import numpy as np
from relight_3d_gl import GL, SourceTree
from weather_cpu_check import ROOT, compiler_command, prepare
from weather_particles_gl_check import run

OUT = ROOT / '.tmp/weather_port'
BASE = '2d4d26c434c'


def dist(values):
    return dict(zip(('p50', 'p95', 'max'), map(float, np.percentile(values, [50, 95, 100]))))


def per_frame_gate(tiles, draws, triangles, milliseconds=0):
    assert tiles <= 1 and draws <= 16 and triangles <= 2048 and milliseconds <= 1.5, (
        'per-frame refresh gate: ' + str(dict(tiles=tiles, draws=draws,
                                            triangles=triangles, ms=milliseconds)))


def native_limits():
    out = OUT / 'budget/native_limits'
    prepare(out)
    (out / 'lldrawbudget.h').write_text((ROOT / 'indra/llrender/lldrawbudget.h').read_text(encoding='utf-8'), encoding='utf-8')
    source = out / 'limits.cpp'
    source.write_text(r'''
#include "lldrawbudget.h"
#include <cassert>
#include <vector>
int main(){
 uint64_t cursor=0;unsigned total=0,frames=0;
 // One oversized batch must split, preserve every triangle and finish.
 do {
  LLDrawBudget budget(cursor);unsigned offset=0,count=30000;
  if(LLDrawBudget::take(0,offset,count)){assert(offset==total);total+=count;}
  assert(budget.draws()<=16 && budget.triangles()<=2048);++frames;
  if(budget.complete())break;
 }while(frames<100);
 assert(total==30000 && frames>1 && frames<100);
 cursor=0;total=0;frames=0;
 do {
  LLDrawBudget budget(cursor);
  for(unsigned i=0;i<71;++i){unsigned offset=0,count=6;if(LLDrawBudget::take(0,offset,count))total+=count;}
  assert(budget.draws()<=16 && budget.triangles()<=2048);++frames;
  if(budget.complete())break;
 }while(frames<100);
 assert(total==71*6 && frames>=5 && frames<100);
 assert(LLDrawBudget::active==nullptr);
 unsigned offset=0,count=30000;assert(LLDrawBudget::take(0,offset,count)&&count==30000);
}
''', encoding='utf-8')
    exe = compiler_command(out, [source], 'limits')
    subprocess.run([str(exe)], check=True, timeout=30)
    # The native submission entrypoints must actually invoke the tested limiter.
    text = (ROOT / 'indra/llrender/llvertexbuffer.cpp').read_text(encoding='utf-8')
    from weather_compute_fixture import function
    for signature in ('void LLVertexBuffer::drawRange(', 'void LLVertexBuffer::drawRangeFast(',
                      'void LLVertexBuffer::drawArrays(U32 mode, U32 first, U32 count) const'):
        assert 'LLDrawBudget::take(' in function(text, signature), 'vertex budget disconnected'
    print('PASS native large-batch continuation, draw/triangle caps and scope restoration', flush=True)


def mutation():
    # Mutate extracted production code in .tmp only, never the worktree or HEAD.
    import weather_particles_gl_check as particles
    original = particles.SourceTree
    class Unbudgeted:
        def __init__(self, ref): self.tree = SourceTree(ref); self.ref = ref
        def exists(self, path): return self.tree.exists(path)
        def read(self, path):
            text = self.tree.read(path)
            if path.endswith('/alweathershelter.cpp'):
                text = text.replace('mTile = {bit%8, bit/8, 1};', 'mTile = {};')
            if path.endswith('/lldrawbudget.h'):
                text = text.replace('DRAW_LIMIT = 16, TRIANGLE_LIMIT = 2048',
                                    'DRAW_LIMIT = 1000000, TRIANGLE_LIMIT = 100000000')
                text = text.replace('microseconds(250)', 'microseconds(1000000000)')
            return text
    particles.SourceTree = Unbudgeted
    try:
        context = run('WORKTREE', cost=False, profile_only=True, benchmark_context=True)
        context['native'].cover_dirty.argtypes = [C.c_double]*3
        context['native'].cover_dirty(0, 0, 4)
        before = context['counts']().copy()
        gl=context['gl'];query=C.c_uint();gl.gen_queries(1,C.byref(query))
        for _ in range(4):
            gl.begin_query(0x88BF,query);start=time.perf_counter()
            context['native'].cover_prepare(0,0,0,0)
            cpu_ms=(time.perf_counter()-start)*1000;gl.end_query(0x88BF)
            gl.finish();elapsed=C.c_uint64();gl.query_u64(query,0x8866,C.byref(elapsed))
            if context['counts']()[0]>before[0]:break
        after=context['counts']()
        milliseconds=cpu_ms+elapsed.value/1e6
        gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(1,C.byref(query))
        hierarchy = np.zeros(4, np.float64)
        context['native'].cover_minimum(hierarchy.ctypes.data)
        per_frame_gate(hierarchy[3]*64, after[1]-before[1], 2*(after[1]-before[1]),milliseconds)
    finally:
        particles.SourceTree = original


def publication(context):
    native=context['native'];gl=context['gl']
    native.cover_budget.argtypes=[C.c_void_p]
    native.cover_load.argtypes=[C.c_uint]
    native.cover_texture.restype=C.c_uint
    native.cover_animate.argtypes=[C.c_float]
    def frame():
        native.fixture_sync();native.cover_prepare(0,0,0,0)
    def metrics():
        values=np.zeros(5,np.float64);native.cover_budget(values.ctypes.data);return values
    def depth():
        gl.active(0x84C0);gl.bind_tex(0x0DE1,native.cover_texture())
        values=np.empty((512,512),np.float32)
        gl.fn('glGetTexImage',None,C.c_uint,C.c_int,C.c_uint,C.c_uint,C.c_void_p)(0x0DE1,0,0x1902,0x1406,values.ctypes.data)
        return values
    for _ in range(512):
        frame()
        if native.cover_pending()==0:break
    else:raise AssertionError('publication setup did not settle')
    native.cover_load(4000);native.cover_animate(.4)
    partial=0;peak_bound=0.
    for _ in range(16):
        before=context['counts']().copy();work=metrics();previous=depth()
        frame()
        after=context['counts']();current=metrics();peak_bound=max(peak_bound,current[3])
        per_frame_gate(after[0]-before[0],after[1]-before[1],current[0]-work[0])
        assert native.cover_ready(),'pending refresh removed valid publication'
        if not current[2] and after[0]>before[0]:
            partial+=1
            assert np.array_equal(previous,depth()),'incomplete tile changed published depth'
    assert partial,'dense caster never exercised partial tile continuation'
    native.cover_load(2);native.cover_animate(1)
    for _ in range(512):
        frame()
        if native.cover_pending()==0:break
    else:raise AssertionError('dense caster follow-up did not settle')
    print('PASS dense caster caps and previous-valid depth during '+str(partial)+' partial frames; bound '+str(peak_bound)+' s',flush=True)


def measure(contexts, samples=480, dual=False):
    gl = next(iter(contexts.values()))['gl']
    for tick in range(128):
        for context in contexts.values():
            context['native'].fixture_sync()
            context['native'].particles_cost_frame(tick, int(dual))
    gl.finish()
    queries = (C.c_uint*(samples*len(contexts)))()
    gl.gen_queries(len(queries), queries)
    cpu = {key: [] for key in contexts}; gpu = {key: [] for key in contexts}
    refreshed = {key: [] for key in contexts}; order = []; stale=[]
    contexts['WORKTREE']['native'].cover_budget.argtypes=[C.c_void_p]
    for tick in range(samples):
        keys = list(contexts) if tick%2 == 0 else list(reversed(contexts))
        for key in keys:
            context = contexts[key]; context['reset'](); context['native'].fixture_sync()
            before = context['counts']().copy()
            query = queries[len(order)]; order.append(key)
            gl.begin_query(0x88BF, query); start = time.perf_counter()
            context['native'].particles_cost_frame(128+tick, int(dual))
            cpu[key].append((time.perf_counter()-start)*1000); gl.end_query(0x88BF)
            after = context['counts']()
            refreshed[key].append(after[0] > before[0])
            if key == 'WORKTREE':
                age=np.zeros(5,np.float64);context['native'].cover_budget(age.ctypes.data);stale.append(age[3:5])
                hierarchy = np.zeros(4, np.float64)
                context['native'].cover_minimum(hierarchy.ctypes.data)
                per_frame_gate((after[0]-before[0])*hierarchy[3]*64,
                               after[1]-before[1], 2*(after[1]-before[1]))
    gl.finish()
    for query, key in zip(queries, order):
        value = C.c_uint64(); gl.query_u64(query, 0x8866, C.byref(value))
        gpu[key].append(value.value/1e6)
    gl.fn('glDeleteQueries', None, C.c_int, C.c_void_p)(len(queries), queries)
    total = {key: np.asarray(cpu[key])+gpu[key] for key in contexts}
    delta = {key: np.maximum(0, total[key]-total['off']) for key in (BASE, 'WORKTREE')}
    records = {key: {'cpu_ms': dist(cpu[key]), 'gpu_ms': dist(gpu[key]),
                     'total_ms': dist(total[key])} for key in contexts}
    for key in delta:
        records[key]['above_off_ms'] = dist(delta[key])
        selected = delta[key][np.asarray(refreshed[key])]
        records[key]['refresh_above_off_ms'] = dist(selected if len(selected) else [0.])
        records[key]['refresh_frames'] = int(sum(refreshed[key]))
    records['WORKTREE']['staleness_bound_seconds_max']=float(np.max(np.asarray(stale)[:,0]))
    records['WORKTREE']['oldest_dirty_seconds_max']=float(np.max(np.asarray(stale)[:,1]))
    return dict(dual=dual, samples=samples, runs=records,
                paired_total_ratio=dist(total['WORKTREE']/total[BASE]),
                paired_above_off_ratio=dist((delta['WORKTREE']+.001)/(delta[BASE]+.001)))


def shelter_measure(contexts,samples=240,dual=False):
    gl=next(iter(contexts.values()))['gl']
    for tick in range(256):
        for key,context in contexts.items():
            context['native'].shelter_sync()
            context['native'].shelter_cost_frame(int(key.endswith('on')),int(dual),tick)
    gl.finish()
    queries=(C.c_uint*(samples*len(contexts)))();gl.gen_queries(len(queries),queries)
    cpu={key:[] for key in contexts};gpu={key:[] for key in contexts};refresh={key:[] for key in contexts};order=[]
    for tick in range(samples):
        for key in (list(contexts) if tick%2==0 else list(reversed(contexts))):
            context=contexts[key];native=context['native'];native.shelter_sync()
            before=context['counts']().copy()
            query=queries[len(order)];order.append(key)
            gl.begin_query(0x88BF,query);start=time.perf_counter()
            native.shelter_cost_frame(int(key.endswith('on')),int(dual),256+tick)
            cpu[key].append((time.perf_counter()-start)*1000);gl.end_query(0x88BF)
            after=context['counts']();refresh[key].append(after[0]>before[0])
            if key=='WORKTREE_on':
                per_frame_gate(after[0]-before[0],after[1]-before[1],2*(after[1]-before[1]))
    gl.finish()
    for query,key in zip(queries,order):
        value=C.c_uint64();gl.query_u64(query,0x8866,C.byref(value));gpu[key].append(value.value/1e6)
    gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(len(queries),queries)
    total={key:np.asarray(cpu[key])+gpu[key] for key in contexts}
    runs={key:dict(cpu_ms=dist(cpu[key]),gpu_ms=dist(gpu[key]),total_ms=dist(total[key])) for key in contexts}
    delta={}
    for ref in (BASE,'WORKTREE'):
        delta[ref]=np.maximum(0,total[ref+'_on']-total[ref+'_off'])
        selected=delta[ref][np.asarray(refresh[ref+'_on'])]
        runs[ref+'_on'].update(above_basic_ms=dist(delta[ref]),refresh_above_basic_ms=dist(selected if len(selected) else [0.]))
    return dict(dual=dual,samples=samples,runs=runs,
                paired_total_ratio=dist(total['WORKTREE_on']/total[BASE+'_on']),
                paired_above_basic_ratio=dist((delta['WORKTREE']+.001)/(delta[BASE]+.001)))


def refresh_measure(contexts,advanced=False,samples=480):
    """Raw refresh only: no rain/material/particle consumer subtraction."""
    gl=next(iter(contexts.values()))['gl']
    def work(context,tick):
        native=context['native']
        if advanced:native.shelter_cost_refresh(1,tick)
        else:native.particles_cost_refresh(tick)
    for tick in range(256):
        for context in contexts.values():
            if advanced:context['native'].shelter_sync()
            else:context['native'].fixture_sync()
            work(context,tick)
    gl.finish()
    queries=(C.c_uint*(samples*len(contexts)))();gl.gen_queries(len(queries),queries)
    cpu={key:[] for key in contexts};gpu={key:[] for key in contexts};order=[]
    for tick in range(samples):
        for key in (list(contexts) if tick%2==0 else list(reversed(contexts))):
            context=contexts[key];native=context['native']
            if advanced:native.shelter_sync()
            else:native.fixture_sync()
            before=context['counts']().copy()
            query=queries[len(order)];order.append(key)
            gl.begin_query(0x88BF,query);start=time.perf_counter();work(context,256+tick)
            cpu[key].append((time.perf_counter()-start)*1000);gl.end_query(0x88BF)
            after=context['counts']()
            if key=='WORKTREE':per_frame_gate(after[0]-before[0],after[1]-before[1],2*(after[1]-before[1]))
    gl.finish()
    for query,key in zip(queries,order):
        value=C.c_uint64();gl.query_u64(query,0x8866,C.byref(value));gpu[key].append(value.value/1e6)
    gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(len(queries),queries)
    total={key:np.asarray(cpu[key])+gpu[key] for key in contexts}
    runs={key:dict(cpu_ms=dist(cpu[key]),gpu_ms=dist(gpu[key]),refresh_ms=dist(total[key])) for key in contexts}
    return dict(advanced=advanced,samples=samples,runs=runs,paired_total_ratio=dist(total['WORKTREE']/total[BASE]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mutation', action='store_true')
    parser.add_argument('--no-cost', action='store_true')
    args = parser.parse_args()
    if args.mutation:
        mutation(); raise AssertionError('unbudgeted full refresh survived')
    native_limits()
    process = subprocess.run([os.sys.executable, __file__, '--mutation'], capture_output=True,
                             text=True, encoding='utf-8', errors='replace', timeout=240)
    (OUT / 'budget_full_refresh_mutation.log').write_text(process.stdout+process.stderr, encoding='utf-8')
    assert process.returncode and 'per-frame refresh gate:' in process.stderr, process.stdout+process.stderr
    print('PASS unbudgeted full-refresh mutation rejected by per-frame gate', flush=True)
    if args.no_cost:
        publication(run('WORKTREE',cost=False,profile_only=True,benchmark_context=True))
        return
    gl = GL(); contexts = {}
    for key, ref in ((BASE, BASE), ('WORKTREE', 'WORKTREE'), ('off', 'WORKTREE')):
        context = run(ref, cost=False, profile_only=True, benchmark_context=True, gl=gl, context_key=key)
        context['set']('SnowEnabled', int(key != 'off'))
        context['set']('PrecipVisibilityEnabled', int(key != 'off'))
        contexts[key] = context
    publication(contexts['WORKTREE'])
    records = []
    for dual in (False, True):
        record = measure(contexts, dual=dual); records.append(record)
        print(json.dumps(record), flush=True)
    (OUT/'REFRESH_BUDGET_WP3_COST.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    from weather_shelter_gl_check import run as shelter_run
    shelter_contexts={}
    for ref in (BASE,'WORKTREE'):
        for on in (False,True):
            key=ref+('_on' if on else '_off')
            shelter_contexts[key]=shelter_run(ref,cost=True,benchmark_context=True,gl=gl,context_key=key)
    shelter_records=[]
    for dual in (False,True):
        row=shelter_measure(shelter_contexts,dual=dual);shelter_records.append(row)
        print('WP7 '+json.dumps(row),flush=True)
    refresh_records={
        'wp3':refresh_measure({ref:contexts[ref] for ref in (BASE,'WORKTREE')}),
        'wp7':refresh_measure({ref:shelter_contexts[ref+'_on'] for ref in (BASE,'WORKTREE')},advanced=True)}
    print('REFRESH '+json.dumps(refresh_records),flush=True)
    current = records[0]['runs']['WORKTREE']
    passed = current['above_off_ms']['p95'] <= 1.0 and all(
        row['runs']['WORKTREE']['refresh_ms']['max']<=1.5 for row in refresh_records.values())
    control = records[0]['runs']['off']['gpu_ms']
    baseline = records[0]['runs'][BASE]['gpu_ms']
    shared = any(row['p95'] > max(.1, row['p50']*2) or row['max'] > max(1.5, row['p50']*4)
                 for row in (control, baseline))
    result = dict(status='PENDING-QUIET' if shared else 'PASS' if passed else 'FAIL',
                  baseline_ref=BASE, records=records, shelter_records=shelter_records, refresh_records=refresh_records,
                  gates={'wp3_main_above_off_p95_ms': 1.0, 'single_frame_refresh_ms': 1.5},
                  functional_mutation='PASS', shared_gpu_variance=shared, timing_gates_passed=passed,
                  scope='Paired/interleaved reversed-order A/B/off; CPU submission + GPU elapsed. '
                        'Production weather owners and shaders; 256-AABB cull/sort services simulated. '
                        'Native viewer octree/VBO cost unmeasured.')
    (OUT / 'REFRESH_BUDGET_COST.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(result['status'], flush=True)
    assert passed and not shared, 'refresh timing gates not accepted: ' + result['status']


if __name__ == '__main__':
    main()
