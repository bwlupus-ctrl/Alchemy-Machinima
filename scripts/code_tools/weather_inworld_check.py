"""In-world regressions using production Director callbacks, presets and bridge.

No viewer build or GL context. Historical refs are read with git show; HEAD never
moves. --source-ref HEAD must reject the original snow-mode and storm-range bugs.
"""
import argparse
import ctypes as C
import re
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET

from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT, SOURCES, compiler_command, prepare
from weather_preparation_fixture import function
from weather_binding_check import build, settings_table


def flows(tree, out):
    native = build(out / 'bindings', source_ref=tree.ref)
    table = settings_table(tree.read('indra/newview/app_settings/settings.xml'))
    for key, info in table.items():
        if key.startswith('AlchemyWeather') and info['Type'] in ('Boolean', 'U32', 'F32'):
            native.set(key.encode(), float({'true':'1', 'false':'0'}.get(info['Value'], info['Value'])))
    native.set(b'AlchemyWeatherEnabled', 1)
    native.set(b'AlchemyWeatherSnowEnabled', 1)
    native.command(0, 5, b'')  # Commit the actual Falling snow control.
    failures = []
    if not (native.get(b'AlchemyWeatherControlMode') == 1 and
            native.get(b'AlchemyWeatherLocalEnabled') == 1 and
            native.get(b'AlchemyWeatherPrecipType') == 1):
        failures.append('Falling snow user action leaves Existing rain/local/type gates closed')
    native.set(b'AlchemyWeatherControlMode', 0)
    native.set(b'AlchemyWeatherPrecipType', 2)
    native.set(b'AlchemyWeatherSnowEnabled', 0)
    native.command(0, 6, b'')
    if not (native.get(b'AlchemyWeatherControlMode') == 1 and native.get(b'AlchemyWeatherSnowEnabled') == 1):
        failures.append('Precipitation type action cannot activate mixed snowfall')
    native.set(b'AlchemyWeatherLightningEnabled', 1)
    # Isolate the manual Library route from the independent source-selection
    # assertions above. Existing rain consumes manual requests in renderWeather.
    native.set(b'AlchemyWeatherControlMode', 1)
    native.set(b'AlchemyWeatherLocalEnabled', 1)
    native.set(b'AlchemyWeatherLightningTiming', 0)
    native.set(b'AlchemyWeatherLightningRate', 0)
    native.set(b'AlchemyWeatherStorm', 0)
    native.command(0, 8, b'')
    if native.get(b'AlchemyWeatherLightningTrigger') != 1:
        failures.append('Strike now button does not request its one-shot')
    observed=(C.c_double*8)();native.trace(observed)
    if observed[4] <= 0: failures.append('Strike now request is lost by production idle bridge')
    native.set(b'AlchemyWeatherLightningTrigger', 0)
    native.set(b'AlchemyWeatherLightningEnabled', 0)
    native.command(0, 8, b'')
    if native.get(b'AlchemyWeatherLightningTrigger') != 0:
        failures.append('Strike now bypasses Lightning enable')
    prepare(out / 'flow')
    sources = []
    for name in SOURCES:
        for ext in ('.h', '.cpp'):
            path = out / 'flow' / (name + ext)
            path.write_text(tree.read('indra/newview/' + name + ext), encoding='utf-8')
            if ext == '.cpp': sources.append(path)
    main = out / 'flow' / 'flow.cpp'
    main.write_text(r'''
#include "alweatheruimodel.h"
#include <iostream>
#include <cmath>
int main() {
 ALWeatherBridge::Settings s; s.enabled=true;
 ALWeatherUIModel::applyPreset(s,7);
 ALWeatherBridge::Controller snow;
 ALWeatherModel::Config lightning; lightning.mEnabled=true;
 for(U64 f=0;f<=1800;++f) snow.update(f,f/60.,s,0,0,1,lightning,{});
 if(!snow.precipitationActive() || ALWeatherBridge::snowAmount(snow.state())<=0.f || snow.state().rainFall!=0.f) {
  std::cerr<<"master -> Apply Heavy snow -> 1800 idle frames fails\n";return 1;}
 ALWeatherUIModel::applyPreset(s,3);s.lightningTiming=1;s.storm=1;
 ALWeatherBridge::Controller storm;bool scheduled=false,manual=false;int invalid=0;
 lightning.mMinDistanceMeters=45;lightning.mMaxDistanceMeters=140;
 for(U64 f=0;f<=7200;++f) {
  storm.update(f,f/60.,s,0,0,1,lightning,{});
  const auto& strike=storm.lightning();
  if(strike.mActive){scheduled=true;
   if(strike.mStrikeDistanceMeters<45 || strike.mStrikeDistanceMeters>140)++invalid;}
 }
 if(!scheduled){std::cerr<<"Downpour + enabled Weather storm has no strike within 120 s\n";return 1;}
 if(invalid){std::cerr<<"storm ignores native lightning distance range: "<<invalid<<" frames\n";return 1;}
 storm.update(7201,120.1,s,0,0,1,lightning,{},true);
 manual=storm.lightning().mActive;
 if(!manual){std::cerr<<"Strike now lost in Weather storm timing\n";return 1;}
 s.lightningTiming=0;lightning.mRatePerMinute=60;
 storm.update(7202,120.2,s,0,0,1,lightning,{},true);
 if(!storm.lightning().mActive){std::cerr<<"Strike now lost in Existing timing\n";return 1;}
 lightning.mMinDistanceMeters=256;lightning.mMaxDistanceMeters=140;
 auto safe=ALWeatherModel::sanitizeConfig(lightning);
 if(safe.mMinDistanceMeters>safe.mMaxDistanceMeters || safe.mMaxDistanceMeters!=140) {
  std::cerr<<"inverted saved range expands beyond selected maximum\n";return 1;}
 std::cout<<"PASS Heavy snow simulation; bounded scheduled storm; manual both schedulers; saved range\n";
}
''', encoding='utf-8')
    executable = compiler_command(out / 'flow', [*sources, main], 'flow')
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    print(result.stdout, end='')
    if result.returncode: failures.append(result.stderr.strip())
    assert not failures, '\n'.join(failures)
    print('PASS production Falling snow / Precipitation / Strike now callbacks and idle consumer')


def listeners(tree):
    # Resolve callbacks (including local lambda aliases), then follow named
    # callees. Inspect the whole closure body: deferred idle calls cannot hide a
    # forbidden reload. Unknown external service calls are outside this graph.
    texts = [tree.read('indra/newview/' + name) for name in
             ('llviewercontrol.cpp', 'alpanelweathersettings.cpp', 'alweatheruimodel.cpp')]
    from director_kit_gen import quick_unsafe_listener_settings
    # This existing unrelated RLV name is a constant, resolved identically by
    # the generator's own central-listener selftest.
    unsafe = quick_unsafe_listener_settings(texts[0].replace('RlvSettingNames::Main', '"RestrainedLove"'))
    assert not any(key.startswith('AlchemyWeather') for key in unsafe), 'weather setting reload/unresolved listener: ' + str(sorted(key for key in unsafe if key.startswith('AlchemyWeather')))
    bodies = {}
    definition = re.compile(r'([A-Za-z_]\w*(?:::\w+)*)\s*\([^;{}]*\)\s*(?:const\s*)?\{')
    for text in texts:
        for match in definition.finditer(text):
            name = match[1].split('::')[-1]
            if name in {'if', 'for', 'while', 'switch', 'catch'}: continue
            bodies.setdefault(name, []).append(function(text[match.start():], match[0][:-1].rstrip()))
        for match in re.finditer(r'\bauto\s+(\w+)\s*=\s*\[[^\]]*\]\s*\([^)]*\)\s*\{', text):
            bodies.setdefault(match[1], []).append(function(text[match.start():], match[0][:-1].rstrip()))
    roots = [body for name in ('setupWeatherMaterialListeners', 'initializeLibraryWeather') for body in bodies.get(name, [])]
    for text in texts:
        for match in re.finditer(r'\bsetting_setup_signal_listener\s*\(', text):
            start = match.start(); end = match.end(); depth = 1
            while depth:
                depth += (text[end] == '(') - (text[end] == ')'); end += 1
            statement = text[start:end]
            if 'AlchemyWeather' in statement: roots.append(statement)
    def visit(text, trail, seen):
        assert not re.search(r'\b(setShaders|handleSetShaderChanged)\s*\(', text), 'weather listener reaches shader reload: ' + ' -> '.join(trail)
        for name in set(re.findall(r'\b([A-Za-z_]\w*)\s*(?:\(|[,\)])', text)) & bodies.keys():
            if name in seen: continue
            for body in bodies[name]: visit(body, trail + [name], seen | {name})
    for root in roots: visit(root, ['weather setting'], set())
    mgr = tree.read('indra/newview/llviewershadermgr.cpp')
    for signature in ('bool LLViewerShaderMgr::loadShadersDeferred()', 'LLViewerShaderMgr::LLViewerShaderMgr()'):
        body = function(mgr, signature)
        assert not re.search(r'getBOOL\(\s*"AlchemyWeather', body), 'weather setting changes startup shader inventory'
    selector = function(tree.read('indra/llrender/llshadermgr.cpp'), 'void LLShaderMgr::prepareWeatherFeatures(')
    assert 'weatherMaterialsSupported()' in selector and 'weatherMaterialsEnabled()' not in selector
    print('PASS weather listener call graph and setting-independent resident shader inventory')


def ui(tree):
    xml = ET.fromstring(tree.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml'))
    for combo in xml.iter('combo_box'):
        assert not {'decimal_digits', 'increment', 'max_val', 'min_val'} & combo.attrib.keys(), 'numeric attributes on combo produce XUI parse warnings'
    director = function(tree.read('indra/newview/llfloaterdirector.cpp'), 'void LLFloaterDirector::bindWeatherPage(')
    assert 'setMaxValue(gSavedSettings.getF32("AlchemyWeatherLightningMaxDistance"))' in director
    assert 'setMinValue(gSavedSettings.getF32("AlchemyWeatherLightningMinDistance"))' in director
    print('PASS generated combo schema and non-inverting lightning rows')


def startup(tree, out):
    factory = function(tree.read('indra/newview/llviewershadermgr.cpp'), 'bool LLViewerShaderMgr::loadShadersDeferred()')
    assert factory.count('.preloadShaders();') == 3, 'deferred inventory does not preload all weather owners'
    for file, signatures in (
            ('alweatherparticles.cpp', ('void ALWeatherParticles::release(', 'bool ALWeatherParticles::initialize(',
                                       'void ALWeatherParticles::prepare(', 'void ALWeatherParticles::render(')),
            ('alweatherresources.cpp', ('void ALWeatherResources::prepare(', 'void ALWeatherResources::prepareIntervals(',
                                       'bool ALWeatherResources::allocate(', 'void ALWeatherResources::release(',
                                       'void ALWeatherShelterMaps::prepare(', 'void ALWeatherShelterMaps::release('))):
        text = tree.read('indra/newview/' + file)
        for signature in signatures:
            body = function(text, signature)
            assert not re.search(r'\b(createShader|preloadShaders|unload)\s*\(', body), 'runtime resource path compiles/unloads a program: ' + signature
    # Compile the actual native owners/preload methods with inert function
    # pointers. No WGL context, OpenGL DLL call, shader compilation or GPU work.
    from weather_particles_fixture import build as owner_build
    class NoGL:
        def __init__(self): self.callbacks = []
        def fn(self, name, result, *args):
            callback = C.CFUNCTYPE(result, *args)(lambda *unused: 0 if result else None)
            self.callbacks.append(callback)
            return callback
    owner_build(tree, out, NoGL(), compile_only=True)
    print('PASS native owner/preload C++ compilation with inert GL; no toggle compiles or unloads programs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-ref', default='WORKTREE')
    parser.add_argument('--check', choices=['flows', 'listeners', 'ui', 'startup'])
    args = parser.parse_args(); tree = SourceTree(args.source_ref)
    out = ROOT / '.tmp/weather_port/inworld_repro' / args.source_ref
    failures = []
    for name, check in [('flows', lambda t: flows(t, out)), ('listeners', listeners), ('ui', ui), ('startup', lambda t: startup(t, out / 'owner_compile'))]:
        if args.check and args.check != name: continue
        try: check(tree)
        except AssertionError as error: failures.append(name + ': ' + str(error))
    assert not failures, '\n'.join(failures)


if __name__ == '__main__': main()
