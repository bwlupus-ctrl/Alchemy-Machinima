"""FIX2 regressions against the immutable reviewed commit, with real WGL events."""
import argparse
import ast
import json
import os
os.environ['PYTHONUTF8']='1'
import subprocess
import sys
from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT

BASE='80abb6c19d6'
ASSERTIONS={1:'outside-volume movement invalidates cover',2:'five 20-ms refreshes passed the budget',
            3:'running texture animation retains stale canopy holes',
            4:'two-metre opening disappears during camera motion',5:'simultaneous sun/moon adds a second light'}


def check(ref,item):
    if item==2:
        source=SourceTree(ref).read('scripts/code_tools/weather_particles_gl_check.py')
        node=next(n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef) and n.name=='enforce_cost')
        scope={};exec(compile(ast.get_source_segment(source,node),'<production cost gate>','exec'),scope)
        example={'memory_bytes':3145728,'cost':[{'on':True,'cpu_plus_gpu_ms':{'p95':.3},
            'refresh_cpu_plus_gpu_ms':{'max':20.}}], 'main_plus_remote_cpu_plus_gpu_ms':{'p95':.3},
            'main_plus_remote_refresh_cpu_plus_gpu_ms':{'max':20.}}
        try:scope['enforce_cost'](example)
        except AssertionError:pass
        else:raise AssertionError(ASSERTIONS[2])
        # The exact 600-frame counterexample, not an invented p95 value.
        import numpy as np
        assert np.percentile([.3]*595+[20.]*5,95)==.3
    else:
        from weather_particles_gl_check import run
        run(ref,cost=False,only_item=item)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-ref',default='WORKTREE')
    p.add_argument('--item',type=int,choices=list(ASSERTIONS));p.add_argument('--before-after',action='store_true');a=p.parse_args()
    results=[]
    if a.item:
        check(a.source_ref,a.item)
        print('PASS FIX2 item',a.item,flush=True)
        return
    for item in ([a.item] if a.item else ASSERTIONS):
        if a.before_after:
            run=subprocess.run([sys.executable,__file__,'--source-ref',BASE,'--item',str(item)],capture_output=True,text=True,encoding='utf-8',timeout=240)
            (ROOT/'.tmp/weather_port'/f'fix2_before_{item}.log').write_text(run.stdout+run.stderr,encoding='utf-8')
            assert run.returncode and ASSERTIONS[item] in run.stderr,'baseline did not reach the named regression: '+run.stdout+run.stderr
        current=subprocess.run([sys.executable,__file__,'--source-ref',a.source_ref,'--item',str(item)],capture_output=True,text=True,encoding='utf-8',timeout=240)
        (ROOT/'.tmp/weather_port'/f'fix2_after_{item}.log').write_text(current.stdout+current.stderr,encoding='utf-8')
        assert current.returncode==0,current.stdout+current.stderr
        results.append({'item':item,'before':'FAIL (expected)' if a.before_after else None,'after':'PASS'})
        print('PASS FIX2 item',item,flush=True)
    (ROOT/'.tmp/weather_port/WP3_FIX2_CHECKS.json').write_text(json.dumps(results,indent=2),encoding='utf-8')


if __name__=='__main__':main()
