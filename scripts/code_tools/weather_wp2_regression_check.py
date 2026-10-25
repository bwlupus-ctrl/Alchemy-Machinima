"""WP2 master/reload, cover-before-mirror and pinned-source regression checks."""
import argparse
import inspect
import os
os.environ['PYTHONUTF8']='1'
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT, SOURCES
from weather_preparation_fixture import function
import weather_binding_check as binding
import weather_material_gl_check as material


def master_check(tree, out, mutation):
    native=binding.build(out,mutation,tree.ref)
    native.set(b'AlchemyWeatherPersistentWetEnabled',1)
    native.set(b'AlchemyWeatherEnabled',1)
    assert native.surface_caps(32,1)==1 and native.surface_bind_count()==1
    native.set(b'AlchemyWeatherEnabled',0)
    assert native.surface_caps(32,1)==0, 'master off retains weather shader feature'
    assert native.surface_bind_count()==0, 'master off enters bindWeatherMaterial'
    listener='setting_setup_signal_listener(gSavedSettings, "AlchemyWeatherEnabled", handleSetShaderChanged);'
    assert listener in tree.read('indra/newview/llviewercontrol.cpp'), 'master toggle lacks shader reload listener'
    print('master feature / upload / reload PASS',flush=True)


def display_check(tree, out, mutation):
    text=tree.read('indra/newview/llviewerdisplay.cpp')
    if mutation=='mirror-order':
        cover='gPipeline.generateWeatherSurfaceCover(*LLViewerCamera::getInstance());'
        text=text.replace(cover,'').replace('gPipeline.mHeroProbeManager.renderProbes();',
                                          'gPipeline.mHeroProbeManager.renderProbes();'+cover)
    display=function(text,'void display(')
    cover=display.index('gPipeline.generateWeatherSurfaceCover(')
    hero=display.index('gPipeline.mHeroProbeManager.renderProbes();')
    assert cover<hero, 'hero probes capture before current-frame cover'
    assert display.count('gPipeline.generateWeatherSurfaceCover(')==1, 'cover prepared more than once'
    assert '!for_snapshot' in display[cover-150:cover] and 'gFrameCount > 1' in display[cover-150:cover]
    # Preserve the pre-existing camera/environment/geometry/GL/cull sequence.
    order=['gPipeline.mHeroProbeManager.renderProbes();','display_update_camera();',
           'LLEnvironment::instance().update(', 'gPipeline.createObjects(',
           'gPipeline.processPartitionQ();','gPipeline.updateGeom(',
           'gPipeline.updateGL();','gPipeline.updateCull(']
    assert [display.index(s) for s in order]==sorted(display.index(s) for s in order), 'camera/geometry order changed'
    print('cover publication before mirror capture / camera geometry order PASS',flush=True)


def pin_check(tree, out, mutation):
    ref='c99e8520ab0'
    original_read=Path.read_text
    def guarded_read(path,*args,**kwargs):
        assert not path.resolve().is_relative_to((ROOT/'indra').resolve()), 'pinned consumer reads working-tree C++ '+str(path)
        return original_read(path,*args,**kwargs)
    kwargs={} if mutation=='source-pin' else {'source_ref':ref}
    with patch.object(Path,'read_text',guarded_read):
        binding.build(out,**kwargs)
    pinned=SourceTree(ref)
    for name in SOURCES:
        for ext in ('.h','.cpp'):
            assert (out/(name+ext)).read_text(encoding='utf-8')==pinned.read('indra/newview/'+name+ext), 'unpinned compiled snapshot'
    command=material.mutation_command(ref,'sampler-order')
    assert command[command.index('--source-ref')+1]==ref, 'mutation subprocess drops source ref'
    # The same ref must reach the material gate's compiled binding/upload code.
    assert ',mutation,ref)' in inspect.getsource(material.run), 'material C++ consumer drops source ref'
    print('pinned native sources / headers / mutation subprocess PASS',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-ref',default='WORKTREE')
    p.add_argument('--check',choices=['master','display','source-pin'])
    p.add_argument('--mutation',choices=['master-switch','mirror-order','source-pin'])
    p.add_argument('--mutations',action='store_true')
    args=p.parse_args();tree=SourceTree(args.source_ref)
    out=ROOT/'.tmp/weather_port/wp2_fix'/(args.mutation or 'baseline')
    for name,check in [('master',master_check),('display',display_check),('source-pin',pin_check)]:
        if args.check is None or name==args.check:check(tree,out/name,args.mutation)
    if args.mutations:
        for check,mutation,expected in [('master','master-switch','master off retains weather shader feature'),
                ('display','mirror-order','hero probes capture before current-frame cover'),
                ('source-pin','source-pin','pinned consumer reads working-tree C++')]:
            r=subprocess.run([sys.executable,__file__,'--source-ref',args.source_ref,'--check',check,'--mutation',mutation],
                             capture_output=True,text=True,encoding='utf-8',timeout=240)
            assert r.returncode!=0 and expected in r.stderr, 'mutation did not reach regression: '+r.stdout+r.stderr
            print('mutation rejected',mutation,expected,flush=True)
