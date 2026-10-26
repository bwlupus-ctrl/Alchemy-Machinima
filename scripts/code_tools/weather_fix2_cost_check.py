"""Paired/interleaved sustained-invalidation cost against immutable WP3 FIX1.

This is a measurement gate, never a substitute for the functional gates.
Render-map membership/sort and 256 world AABBs are simulated; production cover
transactions, region scheduling, hierarchy, particles and GLSL execute unchanged.
"""
import os
os.environ['PYTHONUTF8']='1'
import ctypes as C
import json
import time
from collections import defaultdict
import numpy as np
from relight_3d_gl import GL
from weather_cpu_check import ROOT
from weather_particles_gl_check import run, refresh_counterexample


def distribution(values):
    return dict(zip(('p50','p95','max'),map(float,np.percentile(values,[50,95,100]))))


def profile(context,work):
    native=context['native'];gl=context['gl']
    native.profile_name.argtypes=[C.c_uint];native.profile_name.restype=C.c_char_p
    native.profile_values.argtypes=[C.c_void_p]
    native.profile_start()
    for tick in range(64):work(context,1000+tick)
    native.profile_stop();gl.finish()
    count=native.profile_count();values=np.zeros((count,2),np.float64);native.profile_values(values.ctypes.data)
    grouped=defaultdict(list)
    for i,row in enumerate(values):grouped[native.profile_name(i).decode()].append(row)
    return {name:{'cpu_ms':distribution(np.asarray(rows)[:,0]),'gpu_timestamp_ms':distribution(np.asarray(rows)[:,1]),'calls':len(rows)} for name,rows in grouped.items()}


def main():
    refresh_counterexample()
    gl=GL();contexts={}
    for ref in ('80abb6c19d6','WORKTREE'):
        contexts[ref]=run(ref,cost=False,profile_only=True,benchmark_context=True,gl=gl)
        contexts[ref]['set']('PrecipVisibilityEnabled',1)
        contexts[ref]['set']('SnowEnabled',1)
    def work(context,tick,dual=False):
        context['native'].particles_cost_frame(tick,int(dual))
    records=[]
    for dual in (False,True):
        for tick in range(80):
            for context in contexts.values():context['native'].fixture_sync();work(context,tick,dual)
        gl.finish()
        queries=(C.c_uint*960)();gl.gen_queries(960,queries)
        samples={ref:[] for ref in contexts};refresh={ref:[] for ref in contexts};order=[]
        for tick in range(480):
            for ref in (list(contexts) if tick%2==0 else list(reversed(contexts))):
                context=contexts[ref];context['reset']();context['native'].fixture_sync()
                before=context['counts']()[0]
                query=queries[len(order)];order.append(ref)
                gl.begin_query(0x88BF,query);start=time.perf_counter();work(context,tick+80,dual)
                samples[ref].append((time.perf_counter()-start)*1000);gl.end_query(0x88BF)
                refresh[ref].append(context['counts']()[0]>before)
        # Drain the batch once. Per-sample drains serialize the driver and can
        # charge CPU feed gaps to GPU time; the production owner never drains.
        gl.finish()
        gpu={ref:[] for ref in contexts}
        for query,ref in zip(queries,order):
            value=C.c_uint64();gl.query_u64(query,0x8866,C.byref(value));gpu[ref].append(value.value/1e6)
        gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(960,queries)
        totals={ref:np.asarray(samples[ref])+gpu[ref] for ref in contexts}
        record={'dual':dual,'baseline_ref':'80abb6c19d6','samples':480,'sustained_dirty_frames':480,
            'runs':{ref:{'cpu_ms':distribution(samples[ref]),'gpu_ms':distribution(gpu[ref]),
                'total_ms':distribution(totals[ref]),'refresh_frames':int(sum(refresh[ref])),
                'refresh_ms':distribution(totals[ref][np.asarray(refresh[ref])])} for ref in contexts},
            'paired_total_ratio':distribution(totals['WORKTREE']/totals['80abb6c19d6']),
            'paired_gpu_ratio':distribution(np.asarray(gpu['WORKTREE'])/gpu['80abb6c19d6'])}
        record['passes']=profile(contexts['WORKTREE'],lambda c,t:work(c,t,dual))
        records.append(record);print(json.dumps(record),flush=True)
    current=records[0]['runs']['WORKTREE']['total_ms'];baseline=records[0]['runs']['80abb6c19d6']['gpu_ms']
    passed=all(record['runs']['WORKTREE']['total_ms']['p95']<=.65 and record['runs']['WORKTREE']['refresh_ms']['max']<=.65 for record in records)
    noisy=(baseline['p95']>baseline['p50']*2 or baseline['max']>max(.65,baseline['p50']*4)) and records[0]['runs']['WORKTREE']['refresh_ms']['p50']<=.65
    result={'status':'PASS' if passed else 'PENDING-QUIET' if noisy else 'FAIL','records':records,
        'scope':'Paired A/B/A/B (order reversed each pair), real GPU timestamps + CPU submission, production WP3; geometry/cull/sort services simulated, viewer integration remains unmeasured.'}
    (ROOT/'.tmp/weather_port/WP3_FIX2_PAIRED_COST.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    assert passed,'WP3 <=0.65 ms p95 AND refresh max not accepted: '+result['status']+' '+str(current)


if __name__=='__main__':main()
