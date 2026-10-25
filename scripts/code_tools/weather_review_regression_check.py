"""Behavioral regressions for weather reloads, hero cover lifetime and inactive UI.

Compile production listeners, display scheduling and preparation; viewer services
are counted fixtures. No viewer build. --source-ref can check the pre-fix commit.
"""
import argparse
import ctypes as C
import subprocess

from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT, SOURCES, compiler_command, prepare
from weather_preparation_fixture import function, source as preparation_source


def execute(out, code, name, args=()):
    prepare(out)
    path = out / (name + '.cpp')
    path.write_text(code, encoding='utf-8')
    executable = compiler_command(out, [path], name)
    result = subprocess.run([str(executable), *args], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr


def reload_check(tree, out):
    control = tree.read('indra/newview/llviewercontrol.cpp')
    handler = function(control, 'static bool handleSetShaderChanged(')
    registration = function(control, 'void settings_setup_listeners()')
    if 'static void setupWeatherMaterialListeners()' in control:
        setup = function(control, 'static void setupWeatherMaterialListeners()') + '\nvoid registerListeners(){\n' + '\n'.join(
            line for line in registration.splitlines() if 'setupWeatherMaterialListeners();' in line) + '\n}'
    else:
        setup = 'void registerListeners(){\n' + '\n'.join(
            line for line in registration.splitlines()
            if 'setting_setup_signal_listener' in line and any('"' + key + '"' in line for key in
                ('AlchemyWeatherEnabled', 'AlchemyWeatherPersistentWetEnabled'))) + '\n}'
    code = r'''
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <type_traits>
struct LLSD {};
int destroyed=0,restored=0,reloads=0,releases=0;
bool supported=true;
struct Settings {std::map<std::string,bool> values;
 bool getBOOL(const char* name){return values[name];}} gSavedSettings;
std::map<std::string,std::function<void()>> listeners;
std::vector<std::function<void()>> pending;
void doOnIdleOneTime(std::function<void()> f){pending.push_back(f);}
template<class F>void setting_setup_signal_listener(Settings&,const char* name,F f){
 listeners[name]=[f](){if constexpr(std::is_invocable_v<F>)f();else f(LLSD{});};}
struct Bumps {void destroyGL(){++destroyed;}void restoreGL(){++restored;}}gBumpImageList;
struct Pipeline {bool isInit(){return true;}static void refreshCachedSettings(){}}gPipeline;
struct LLViewerShaderMgr {static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}
 bool weatherMaterialsSupported(){return supported;}void setShaders(){++reloads;++releases;}};
using LLPipeline=Pipeline;
HANDLER
SETUP
void set(const char* name,bool value){if(gSavedSettings.values[name]==value)return;
 gSavedSettings.values[name]=value;listeners.at(name)();}
void flush(){auto work=std::move(pending);pending.clear();for(auto& f:work)f();}
bool count(int expected){flush();if(reloads!=expected||destroyed!=expected||restored!=expected||releases!=expected){
 std::cerr<<"unexpected weather reload counts: "<<reloads<<","<<destroyed<<","<<restored<<","<<releases<<" expected "<<expected;return false;}return true;}
int main(int argc,char**){
 const char* master="AlchemyWeatherEnabled";const char* wet="AlchemyWeatherPersistentWetEnabled";
 if(argc>1){gSavedSettings.values[master]=true;gSavedSettings.values[wet]=true;registerListeners();
  set(master,false);return count(1)?0:1;}
 registerListeners();
 for(int i=0;i<30;++i){set(master,true);if(!count(0))return 1;set(master,false);if(!count(0))return 1;}
 set(wet,true);if(!count(0))return 1;
 set(master,true);set(wet,false);set(wet,true);if(!count(1))return 1;
 set(master,false);set(master,true);if(!count(1))return 1;
 set(master,false);if(!count(2))return 1;
 set(master,true);set(wet,false);if(!count(2))return 1;
 set(wet,true);if(!count(3))return 1;
 supported=false;set(master,false);if(!count(3))return 1;
 set(master,true);if(!count(3))return 1;
}
'''.replace('HANDLER', handler).replace('SETUP', setup)
    execute(out, code, 'reload')
    execute(out, code, 'reload_start_enabled', ('enabled',))


def display_check(tree, out, mutation=None):
    display = function(tree.read('indra/newview/llviewerdisplay.cpp'), 'void display(')
    start = display.index('    if (!gDisconnected && !LLApp::isExiting())', display.index('gPipeline.resetFrameStats()'))
    end = display.index('        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("display - 1");', start)
    schedule = display[start:end] + '\n}'
    if mutation == 'mirror-order':
        cover = 'gPipeline.generateWeatherSurfaceCover(*LLViewerCamera::getInstance());'
        schedule = schedule.replace(cover, '').replace('gPipeline.mHeroProbeManager.renderProbes();',
                                                       'gPipeline.mHeroProbeManager.renderProbes();' + cover)
    # Derive invalidation from the production lifecycle, not an order assertion.
    hero = function(tree.read('indra/newview/llheroprobemanager.cpp'), 'void LLHeroProbeManager::update()')
    init_start = hero.index('    if (!mInitialized &&')
    initialization = function(hero[init_start:], 'if (!mInitialized &&')
    shader = function(tree.read('indra/newview/llviewershadermgr.cpp'), 'void LLViewerShaderMgr::setShaders()')
    release = function(tree.read('indra/newview/pipeline.cpp'), 'void LLPipeline::releaseGLBuffers()')
    release_calls = '\n'.join(line for line in release.splitlines() if 'mWeatherSurfaces.release();' in line)
    shader_calls = '\n'.join(line for line in shader.splitlines() if 'gPipeline.releaseGLBuffers();' in line)
    code = r'''
#include <iostream>
#define LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY(x)
#define LL_PROFILE_GPU_ZONE(x)
using U32=unsigned;
bool gDisconnected=false,gSnapshot=false,for_snapshot=false;
U32 gFrameCount=2;
constexpr int STATE_PRECACHE=1;
struct LLApp{static bool isExiting(){return false;}};
struct LLStartUp{static int getStartupState(){return 2;}};
struct LLViewerCamera{static LLViewerCamera* getInstance(){static LLViewerCamera c;return &c;}};
int gSavedSettings=0;
template<class T>struct LLCachedControl{LLCachedControl(int&,const char*,T){}operator T(){return T{};}};
int failures=0,publications=0,captures=0,shader_releases=0;
struct Surface{U32 frame=0;void release(){frame=0;++shader_releases;}};
struct Hero{bool mInitialized=false;void update();void renderProbes();};
struct Pipeline{bool RenderMirrors=true;Surface mWeatherSurfaces;Hero mHeroProbeManager;
 void releaseGLBuffers(){RELEASE}
 void generateWeatherSurfaceCover(LLViewerCamera&){++publications;mWeatherSurfaces.frame=gFrameCount;}
 void generateWeatherAdvancedShelter(LLViewerCamera&){}
}gPipeline;
struct LLViewerShaderMgr{static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}
 void clearShaderCache(){}void setShaders(){SHADER}};
void Hero::update(){INITIALIZATION}
void Hero::renderProbes(){++captures;if(gPipeline.mWeatherSurfaces.frame!=gFrameCount){
 ++failures;std::cerr<<"hero probes lack current-frame cover after resource invalidation; ";}}
void frame(){SCHEDULE}
int main(){
 frame();if(shader_releases!=1){std::cerr<<"fixture did not exercise hero shader invalidation";return 1;}
 if(publications!=1||captures!=1||gPipeline.mWeatherSurfaces.frame!=gFrameCount)++failures;
 ++gFrameCount;frame();if(publications!=2||captures!=2||shader_releases!=1)++failures;
 gPipeline.RenderMirrors=false;++gFrameCount;frame();
 if(publications!=3||captures!=2||gPipeline.mWeatherSurfaces.frame!=gFrameCount)++failures;
 for_snapshot=true;gSnapshot=true;++gFrameCount;frame();if(publications!=3)++failures;
 for_snapshot=false;gSnapshot=false;gFrameCount=1;frame();if(publications!=3)++failures;
 return failures?1:0;
}
'''.replace('RELEASE', release_calls).replace('SHADER', shader_calls).replace('INITIALIZATION', initialization).replace('SCHEDULE', schedule)
    execute(out, code, 'display')


def status_check(tree, out):
    prepare(out)
    source = preparation_source(tree, out)
    for name in SOURCES:
        for ext in ('.h', '.cpp'):
            (out / (name + ext)).write_text(tree.read('indra/newview/' + name + ext), encoding='utf-8')
    dll = compiler_command(out, [source, *(out / (n + '.cpp') for n in SOURCES)], 'status', dll=True)
    lib = C.CDLL(str(dll))
    lib.fixture_weather_status.restype = C.c_char_p
    lib.fixture_weather_set.argtypes = [C.c_char_p, C.c_double]
    lib.fixture_weather_step.argtypes = [C.c_double]
    lib.fixture_weather_counts.argtypes = [C.POINTER(C.c_double)]
    def status(): return lib.fixture_weather_status().decode()
    def set_(name, value): lib.fixture_weather_set(('AlchemyWeather' + name).encode(), value)
    def counts():
        data = (C.c_double * 4)();lib.fixture_weather_counts(data);return tuple(data)
    failures = []
    def expect(expected, label):
        if status() != expected: failures.append(label + ': ' + repr(status()) + ' expected ' + expected)
    expect('Weather off', 'startup inactive status')
    set_('Enabled', 1);expect('Existing rain', 'mode-0 master enable before a frame')
    set_('Enabled', 0);expect('Weather off', 'mode-0 master disable before a frame')
    set_('Enabled', 1);set_('ControlMode', 1);set_('LocalEnabled', 1);lib.fixture_weather_step(1.)
    set_('Enabled', 0);lib.fixture_weather_step(2.);expect('Weather off', 'cleanup status')
    set_('ControlMode', 0);set_('Enabled', 1);expect('Existing rain', 'mode-0 reenable after cleanup')
    set_('Enabled', 0);expect('Weather off', 'mode-0 disable after cleanup')
    before = counts()
    for i in range(120): lib.fixture_weather_step(3. + i / 60.)
    assert before == counts(), 'inactive frames perform preparation or status work'
    startup_dll = compiler_command(out, [source, *(out / (n + '.cpp') for n in SOURCES)], 'status_start_enabled', dll=True)
    startup = C.CDLL(str(startup_dll))
    startup.fixture_weather_load.argtypes = [C.c_char_p, C.c_double]
    startup.fixture_weather_status.restype = C.c_char_p
    startup.fixture_weather_load(b'AlchemyWeatherEnabled', 1)
    if startup.fixture_weather_status() != b'Existing rain': failures.append('startup enabled mode-0 status is blank or stale')
    assert not failures, '; '.join(failures)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-ref', default='WORKTREE')
    parser.add_argument('--check', choices=['reload', 'display', 'status'])
    args = parser.parse_args()
    tree = SourceTree(args.source_ref)
    failures = []
    for name, check in [('reload', reload_check), ('display', display_check), ('status', status_check)]:
        if args.check and args.check != name: continue
        try:
            check(tree, ROOT / '.tmp/weather_port/review_fix' / args.source_ref / name)
            print(name + ': PASS', flush=True)
        except AssertionError as error:
            failures.append(name + ': ' + str(error));print(failures[-1], flush=True)
    assert not failures, '\n'.join(failures)
