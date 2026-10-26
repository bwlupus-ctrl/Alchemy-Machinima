"""WP1 review regressions: production preparation, camera fade, presets and Quick.

Run before a fix to record the individual failing checks; --mutations requires
each restored defect to fail its behavioural assertion after the fixes.
"""
import argparse
import ctypes as C
import subprocess
import sys

import numpy as np
from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT, SOURCES, compiler_command, prepare
from weather_preparation_fixture import function, source as preparation_source


def idle_check(out, mutation=None):
    prepare(out)
    source = preparation_source(SourceTree('WORKTREE'), out, mutation)
    dll = compiler_command(out, [source, *(ROOT / ('indra/newview/' + n + '.cpp') for n in SOURCES)], 'idle', dll=True)
    lib = C.CDLL(str(dll))
    lib.fixture_weather_set.argtypes = [C.c_char_p, C.c_double]
    lib.fixture_weather_step.argtypes = [C.c_double]
    lib.fixture_weather_counts.argtypes = [C.POINTER(C.c_double)]
    def set_(name, value): lib.fixture_weather_set(('AlchemyWeather' + name).encode(), value)
    def counts():
        data = np.zeros(4, np.float64)
        lib.fixture_weather_counts(data.ctypes.data_as(C.POINTER(C.c_double)))
        return data
    for enabled, mode in ((0, 0), (1, 0), (0, 1)):
        set_('Enabled', enabled); set_('ControlMode', mode)
        before = counts()
        for f in range(120): lib.fixture_weather_step(f / 60.)
        assert np.array_equal(before, counts()), 'inactive idle performs preparation work: ' + str(counts() - before)
    set_('Enabled', 1); set_('ControlMode', 1); set_('LocalEnabled', 1)
    lib.fixture_weather_step(3.)
    before = counts()
    assert before[0] and before[1] and before[2], 'production idle call disconnected'
    set_('Enabled', 0); lib.fixture_weather_step(3.)
    cleaned = counts()
    assert cleaned[0] > before[0] and cleaned[2] > before[2], 'missing one-time library cleanup'
    for f in range(120): lib.fixture_weather_step(4. + f / 60.)
    assert np.array_equal(cleaned, counts()), 'cleanup repeats while master off'
    # WP2 must still prepare Existing rain when persistent wet surfaces are enabled.
    set_('Enabled', 1); set_('ControlMode', 0); set_('PersistentWetEnabled', 1)
    lib.fixture_weather_step(7.)
    assert counts()[0] > cleaned[0], 'WP2 Existing rain surfaces stopped preparing'


def shelter_check(out, mutation=None):
    prepare(out)
    tree = SourceTree('WORKTREE')
    pipe = tree.read('indra/newview/pipeline.cpp')
    render = function(pipe, 'void LLPipeline::renderWeather(')
    start = render.index('        // An auxiliary camera never edits')
    end = render.index('\n    }', start)
    selection = render[start:end]
    if mutation == 'aux-history':
        selection = 'WeatherShelterExposure fresh; exposed = auxiliary ? fresh.update(now, target_exposed) : sWeatherShelterExposure.update(now, target_exposed);'
    exposure = function(pipe, 'struct WeatherShelterExposure') + ';' if 'struct WeatherShelterExposure' in pipe else 'using WeatherShelterExposure = ALWeatherShelter::Exposure;'
    code = r'''
#include "alweathershelter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
EXPOSURE
WeatherShelterExposure sWeatherShelterExposure, camera_a, camera_b;
WeatherShelterExposure* auxiliary_exposure=&camera_a;
float sample(double now,bool auxiliary,float target_exposed){float exposed=1;
SELECTION
return exposed;}
int main(){
 sample(0,false,1);sample(0,true,1);
 const float main=sample(1./60.,false,0),aux=sample(1./60.,true,0);
 if(!(aux>0 && aux<1 && aux==main)){std::cerr<<"auxiliary roof edge snaps instead of fading";return 1;}
 auxiliary_exposure=&camera_b;
 if(sample(1./60.,true,1)!=1){std::cerr<<"auxiliary camera histories leak";return 1;}
 auxiliary_exposure=&camera_a;
 const float next=sample(2./60.,true,0);
 if(!(next>0 && next<aux)){std::cerr<<"auxiliary fade loses history";return 1;}
 camera_a.reset();
 if(sample(3./60.,true,0)!=0){std::cerr<<"camera lifecycle reset retains history";return 1;}
}
'''.replace('EXPOSURE', exposure).replace('SELECTION', selection)
    source = out / 'shelter.cpp'; source.write_text(code, encoding='utf-8')
    executable = compiler_command(out, [source, *(ROOT / ('indra/newview/' + n + '.cpp') for n in SOURCES)], 'shelter')
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    if 'mWeatherExposure' in tree.read('indra/newview/llprismlens.cpp'):
        prism = tree.read('indra/newview/llprismlens.cpp')
        for signature in ('void releaseRenderResources()', 'void onRenderTargetsReleased()', 'void suppressOutput(U32 slot)'):
            assert 'mWeatherExposure.reset()' in function(prism, signature), 'camera lifecycle misses exposure reset: ' + signature
        assert '&capture->mWeatherExposure' in prism, 'camera-owned shelter history is disconnected'


def binding_checks(out, mutation=None):
    from weather_binding_check import build
    native = build(out, mutation)
    failures = []
    native.set(b'AlchemyWeatherEnabled', 1)
    native.set(b'AlchemyWeatherControlMode', 0)
    native.set(b'AlchemyWeatherRainEnabled', 0)
    native.command(2, 0, b'')
    if native.get(b'AlchemyWeatherRainEnabled') != 0:
        failures.append('library preset changes saved legacy Rain toggle')
    native.set(b'AlchemyWeatherControlMode', 0)
    if native.get(b'AlchemyWeatherRainEnabled') != 0:
        failures.append('mode round trip loses disabled legacy Rain')
    for key, value, which in ((b'AlchemyWeatherSeed', 123456, 0), (b'AlchemyWeatherRainRate', 80, 1), (b'AlchemyWeatherRainRate', 13, 1)):
        if not native.pinned(key, value, which): failures.append('pinned Quick proxy stale: ' + key.decode())
    assert not failures, '; '.join(failures)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mutation', choices=['idle-disconnect', 'aux-history', 'proxy-notify', 'legacy-preset'])
    p.add_argument('--mutations', action='store_true')
    args = p.parse_args()
    out = ROOT / '.tmp/weather_port/wp1_fix' / (args.mutation or 'baseline')
    failures = []
    for name, check in (('idle', idle_check), ('shelter', shelter_check), ('bindings', binding_checks)):
        try:
            check(out / name, args.mutation)
            print(name + ': PASS', flush=True)
        except AssertionError as e:
            failures.append(name + ': ' + str(e))
            print(failures[-1], flush=True)
    assert not failures, '\n'.join(failures)
    if args.mutations:
        mutations = [
            ('weather_wp1_regression_check.py', 'idle-disconnect', 'production idle call disconnected'),
            ('weather_wp1_regression_check.py', 'aux-history', 'auxiliary roof edge snaps'),
            ('weather_wp1_regression_check.py', 'proxy-notify', 'pinned Quick proxy stale'),
            ('weather_wp1_regression_check.py', 'legacy-preset', 'library preset changes saved legacy Rain toggle'),
            ('weather_binding_check.py', 'idle-disconnect', 'no observable production consumer'),
            ('weather_binding_check.py', 'setting', 'no observable production consumer'),
            ('weather_binding_check.py', 'command', 'AssertionError'),
            ('weather_rain_gl_check.py', 'idle-disconnect', 'library rain missing'),
            ('weather_rain_gl_check.py', 'default', 'mode-0 native uploads changed'),
            ('weather_rain_gl_check.py', 'upload', 'mode-0 native uploads changed'),
            ('weather_rain_gl_check.py', 'aux', 'missing auxiliary weather consumer'),
        ]
        for script, mutation, expected in mutations:
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/code_tools' / script), '--mutation', mutation],
                                    capture_output=True, text=True, encoding='utf-8', timeout=240)
            (out / (script + '_' + mutation + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
            assert result.returncode != 0, 'SURVIVING MUTATION: ' + script + '/' + mutation
            assert expected in result.stderr, 'mutation did not reach its behavioural assertion: ' + result.stderr
            print('mutation rejected: ' + script + '/' + mutation + ' (' + expected + ')', flush=True)


if __name__ == '__main__': main()
