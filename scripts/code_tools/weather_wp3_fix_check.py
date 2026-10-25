"""WP3 review contracts, each rejected at immutable reviewed commit abf07777144.

The GL gate supplies the behavioural MRT/ray/cache/lifecycle/light checks. These
contracts additionally catch disconnected production preparation and test gates.
"""
import argparse
import json
import re
from pathlib import Path
from relight_3d_gl import SourceTree
from weather_compute_fixture import declaration, function
from weather_cpu_check import ROOT


def check(tree, item):
    owner=tree.read('indra/newview/alweatherparticles.cpp')
    render=function(owner,'void ALWeatherParticles::render(')
    pipe=tree.read('indra/newview/pipeline.cpp')
    particles=tree.read('scripts/code_tools/weather_particles_gl_check.py')
    if item==1:
        assert 'glDrawBuffers(1, &beauty)' in render and 'glDrawBuffers(buffer_count, draw_buffers.data())' in render,'both draws must isolate and restore MRTs'
    elif item==2:
        veil=tree.read('indra/newview/app_settings/shaders/class1/deferred/weatherPrecipVeilF.glsl')
        assert 'weatherPrecipExposedDistance(start,direction,distance_m)' in veil,'endpoint exposure is not ray length'
        cover=tree.read('indra/newview/app_settings/shaders/class1/deferred/weatherPrecipCoverF.glsl')
        assert 'hi = min(hi,max(t0,t1))' in cover and 'exposed*step_m' in cover,'ray must clip to known coverage and integrate'
    elif item==3:
        cover=function(pipe,'void LLPipeline::generateWeatherSurfaceCover(')
        draw = function(pipe,'bool LLPipeline::drawWeatherCover(') if 'bool LLPipeline::drawWeatherCover(' in pipe else cover
        traversal = 'drawWeatherCover(' if draw != cover else 'renderShadow('
        assert cover.index('mWeatherPrecipitationCoverCache.reuse(')<cover.index(traversal) and 'renderShadow(' in draw,'snow repeats full cover traversal'
        assert 'combined preparation/cover/render p95 exceeds 0.65 ms' in particles and 'max(tails)<=.65' in particles,'cost p95 is not enforced'
        assert 'native.cover_prepare(cell,0,0' in particles and 'combined();cpu.append' in particles,'timed scope omits preparation or cover'
        from weather_particles_gl_check import enforce_cost
        over={'memory_bytes':3145728,'cost':[{'on':True,'cpu_plus_gpu_ms':{'p95':.650001}}],
              'main_plus_remote_cpu_plus_gpu_ms':{'p95':.1}}
        try:enforce_cost(over)
        except AssertionError:pass
        else:raise AssertionError('over-budget p95 accepted')
    elif item==4:
        caps=function(owner,'bool ALWeatherParticles::supported(')
        assert 'mGLVersion >= 4.49f && glTextureBarrier' in caps and 'glTextureBarrier();' in render,'feedback requires exact GL 4.5 barrier semantics'
        assert 'LLGLDepthTest depth_test(GL_TRUE, GL_FALSE' in render and 'LLGLDepthTest no_depth(GL_FALSE, GL_FALSE)' in render,'sampled depth must remain read only'
    elif item==5:
        assert not any(s in render for s in ('initialize(', 'release()', '.unload()')),'render owns synchronous initialization/unload'
        prepare=function(owner,'void ALWeatherParticles::prepare(')
        assert 'initialize(cap)' in prepare and 'mReady &&' in prepare,'session readiness must publish after preparation'
        assert 'gPipeline.mWeatherParticles.prepare()' in tree.read('indra/newview/alpanelweathersettings.cpp'),'pre-frame preparation disconnected'
    elif item==6:
        caps=function(tree.read('indra/llrender/llshadermgr.cpp'),'bool LLShaderMgr::computeShadersSupported(')
        assert caps.index('generation == sContextGeneration')<caps.index('glGetIntegerv('),'limits are not context cached'
        narrow = 'State saved_state(2, 1, false)' in render or ('const U32 cover_units = s.advancedShelter ? 4U : 2U;' in render and 'State saved_state(cover_units, 1, false)' in render)
        assert 'glGetAttribLocation' not in render and narrow,'view path queries attributes or snapshots unrelated state'
        assert 'library-on/WP3-off performs GL work' in particles,'off fixture missing'
    elif item==7:
        hook=function(pipe,'void LLPipeline::renderWeatherParticles(')
        assert all(s in hook for s in ('getTotalAmbient()', 'getSunDiffuse()', 'getMoonDiffuse()', 'getIsSunUp()', 'getIsMoonUp()')),'raw settings bypass evaluated night illumination'
    elif item==8:
        panel=tree.read('indra/newview/alpanelweathersettings.cpp')
        declaration(panel,'sParticleStatus')
        for file in ('weather_binding_check.py','weather_preparation_fixture.py'):
            fixture=tree.read('scripts/code_tools/'+file)
            assert 'declaration(panel, n)' in fixture and 'std::string sParticleStatus;' not in fixture,'fixture manufactures production global '+file
        broken=re.sub(r'^\s*std::string sParticleStatus\s*[^;]*;', '', panel, flags=re.M)
        try:declaration(broken,'sParticleStatus')
        except AssertionError:pass
        else:raise AssertionError('missing declaration was repaired')
        class BrokenTree:
            ref=tree.ref
            def read(self,path):
                return broken if path=='indra/newview/alpanelweathersettings.cpp' else tree.read(path)
        from weather_preparation_fixture import source
        from weather_binding_check import build
        from unittest.mock import patch
        for name,fixture in (('preparation',source),('binding',build)):
            out=ROOT/'.tmp/weather_port/wp3_missing_declaration'/name;out.mkdir(parents=True,exist_ok=True)
            try:
                if name=='preparation':fixture(BrokenTree(),out)
                else:
                    with patch('weather_binding_check.SourceTree',return_value=BrokenTree()):fixture(out,source_ref=tree.ref)
            except AssertionError as error:
                assert 'missing production declaration: sParticleStatus' in str(error),str(error)
            else:raise AssertionError(name+' fixture repaired missing sParticleStatus')
    elif item==9:
        compute=tree.read('scripts/code_tools/weather_compute_gl_check.py')
        assert "SourceTree('330d012e161')" in compute and "'archive','28a5d96de19'" in compute,'moving identity/pre-WP3 baseline'
        assert "'--source-ref',a.source_ref,'--mutation'" in particles,'mutation subprocess loses source ref'
        assert 'lib.graphics(' in compute and 'existing sampler assignments changed' in compute,'actual existing graphics/samplers not checked'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-ref',default='WORKTREE');p.add_argument('--before-after',action='store_true')
    a=p.parse_args();results=[]
    for item in range(1,10):
        if a.before_after:
            try:check(SourceTree('abf07777144'),item)
            except (AssertionError,ValueError):before='FAIL (expected)'
            else:raise AssertionError('reviewed baseline unexpectedly passes item '+str(item))
        else:before=None
        check(SourceTree(a.source_ref),item)
        results.append({'item':item,'reviewed':before,'current':'PASS'})
        print('item',item,'reviewed:',before,'current: PASS',flush=True)
    (ROOT/'.tmp/weather_port/WP3_FIX_CHECKS.json').write_text(json.dumps(results,indent=2),encoding='utf-8')


if __name__=='__main__':main()
