"""Native setting transactions and logout/teleport regressions; no viewer build."""
import argparse
import ctypes as C
import re

from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT, SOURCES, compiler_command, prepare
from weather_preparation_fixture import function, source
from weather_binding_check import settings_table


def build_settings(tree, out, writers=False):
    prepare(out)
    for name in SOURCES:
        for ext in ('.h', '.cpp'):
            (out / (name + ext)).write_text(tree.read('indra/newview/' + name + ext), encoding='utf-8')
    path = source(tree, out)
    if writers:
        code = path.read_text(encoding='utf-8')
        code = code.replace('struct LLSD {};', '''struct LLSD {
 double value=0;std::map<std::string,LLSD> fields;
 LLSD()=default;LLSD(double v):value(v){}
 bool has(const std::string& key)const{return fields.count(key)!=0;}
 LLSD& operator[](const std::string& key){return fields[key];}
 const LLSD& operator[](const std::string& key)const{return fields.at(key);}
};''')
        code = code.replace(' void resetToDefault(bool);', ' void resetToDefault(bool);void set(const LLSD&);void setValue(const LLSD&);')
        code += '\nnamespace WeatherPreparationFixture {\nvoid LLControlVariable::set(const LLSD& v){gSavedSettings.setF32(name,float(v.value));}\nvoid LLControlVariable::setValue(const LLSD& v){set(v);}\nstruct ALScopedPresetMaterializing {};\n'
        presets = tree.read('indra/newview/alkitpresets.cpp')
        code += 'namespace ALPresetLook {\n' + function(presets, 'void reset(const std::vector<std::string>& settings)')
        code += function(presets, 'void apply(const std::vector<std::string>& settings, const LLSD& values)') + '\n}\n'
        panel = tree.read('indra/newview/alpanelweathersettings.cpp')
        named = function(panel, 'bool ALPanelWeatherSettings::applyNamedPreset(')
        replay = function(tree.read('indra/newview/alsettingsundohistory.cpp'), '    void replayModel(')
        def scope(body):
            match = re.search(r'(?:ALPanelWeatherSettings::)?SettingsBatch\s+\w+;', body)
            return match[0].replace('SettingsBatch', 'ALPanelWeatherSettings::SettingsBatch') if match and '::' not in match[0] else (match[0] if match else '')
        named_loop = function(named, 'for (const std::string& setting : trackedSettings())')
        replay_start = replay.index('        auto apply_change =')
        replay_end = replay.index('        mApplying = false;', replay_start)
        replay_writes = replay[replay_start:replay_end]
        code += '''
const std::vector<std::string>& trackedSettings(){return ALPanelWeatherSettings::trackedSettings();}
struct ALSettingsUndoHistoryModel {struct Change {std::string mKey;LLSD mOldValue,mNewValue;};struct Step {std::vector<Change> mChanges;};};
extern "C" __declspec(dllexport) void fixture_range_write(int action,double minimum,double maximum){
 const std::vector<std::string> keys={"AlchemyWeatherLightningMinDistance","AlchemyWeatherLightningMaxDistance"};
 LLSD values;values[keys[0]]=minimum;values[keys[1]]=maximum;
 if(action==0){ALPresetLook::apply(keys,values);return;}
 if(action==1){
 NAMED_SCOPE
 ALPresetLook::reset(trackedSettings());LLSD preset;preset["settings"]=values;
 NAMED_LOOP
 return;}
 const bool use_new_value=action==3;
 ALSettingsUndoHistoryModel::Step step;
 for(const auto& key:keys)step.mChanges.push_back({key,values[key],values[key]});
 REPLAY_SCOPE
 REPLAY_WRITES
}
}
'''.replace('NAMED_SCOPE', scope(named)).replace('NAMED_LOOP', named_loop).replace('REPLAY_SCOPE', scope(replay)).replace('REPLAY_WRITES', replay_writes)
        path.write_text(code, encoding='utf-8')
    native = C.CDLL(str(compiler_command(out, [path, *(out / (n + '.cpp') for n in SOURCES)], 'settings', dll=True)))
    for method in ('set', 'load', 'default'):
        getattr(native, 'fixture_weather_' + method).argtypes = [C.c_char_p, C.c_double]
    native.fixture_weather_get.argtypes = [C.c_char_p]
    native.fixture_weather_get.restype = C.c_double
    native.fixture_weather_step.argtypes = [C.c_double]
    native.fixture_weather_counts.argtypes = [C.POINTER(C.c_double)]
    native.fixture_weather_reload_counts.argtypes = [C.POINTER(C.c_double)]
    native.fixture_weather_lifecycle.argtypes = [C.c_int] * 4
    table = settings_table(tree.read('indra/newview/app_settings/settings.xml'))
    for key, info in table.items():
        if key.startswith('AlchemyWeather') and info['Type'] in ('Boolean', 'U32', 'F32'):
            value = float({'true': '1', 'false': '0'}.get(info['Value'], info['Value']))
            native.fixture_weather_load(key.encode(), value)
            native.fixture_weather_default(key.encode(), value)
    return native, table


def ranges(tree, out):
    native, _ = build_settings(tree, out, writers=True)
    native.fixture_range_write.argtypes = [C.c_int, C.c_double, C.c_double]
    panel = tree.read('indra/newview/alpanelweathersettings.cpp')
    presets = tree.read('indra/newview/alkitpresets.cpp')
    # All production writers must enclose their complete write loop, rather than
    # merely repairing already-corrupted values afterwards.
    missing_scopes = []
    for text, signature in (
        (presets, 'void apply(const std::vector<std::string>& settings, const LLSD& values)'),
        (panel, 'void ALPanelWeatherSettings::resetAllSettingsToDefaults()'),
        (panel, 'bool ALPanelWeatherSettings::applyNamedPreset('),
        (tree.read('indra/newview/alkithistory.cpp'), 'bool ALKitHistory::replay('),
        (tree.read('indra/newview/alsettingsundohistory.cpp'), '    void replayModel(')):
        body = function(text, signature)
        if 'SettingsBatch' not in body or body.index('SettingsBatch') > body.index('for ('):
            missing_scopes.append(signature)
    if 'beginSettingsBatch();' not in function(presets, 'Load::Load(') or 'endSettingsBatch();' not in function(presets, 'Load::~Load('):
        missing_scopes.append('ALPresetLook::Load')
    keys = [b'AlchemyWeatherLightningMinDistance', b'AlchemyWeatherLightningMaxDistance']
    def set_pair(values, reverse=False):
        native.fixture_weather_batch(1)
        for index in ([1, 0] if reverse else [0, 1]):
            native.fixture_weather_set(keys[index], values[index])
        native.fixture_weather_batch(0)
    def pair(): return tuple(native.fixture_weather_get(key) for key in keys)
    # These execute production apply/reset/preset/replay write loops and listeners.
    for label, initial, target, reverse in (
        ('scene load', (45, 130), (200, 300), False),
        ('named preset', (10, 20), (200, 300), False),
        ('undo', (200, 300), (10, 20), True),
        ('redo', (10, 20), (200, 300), False)):
        set_pair(initial)
        action = {'scene load': 0, 'named preset': 1, 'undo': 2, 'redo': 3}[label]
        native.fixture_range_write(action, *target)
        assert pair() == target, label + ' corrupts range: ' + str(pair())
    set_pair((10, 20))
    native.fixture_weather_reset()  # The actual Reset all implementation.
    assert pair() == (45, 130), 'Reset all corrupts defaults: ' + str(pair())
    native.fixture_weather_set(keys[0], 300)
    assert pair() == (130, 130), 'interactive minimum inverts range'
    native.fixture_weather_set(keys[1], 20)
    assert pair() == (20, 20), 'interactive maximum inverts range'
    assert not missing_scopes, 'unbatched writers: ' + str(missing_scopes)
    print('PASS native listeners: scene 200-300, Reset all 45-130, preset, undo/redo, interactive; production writer scopes')


def lifecycle(tree, out):
    native, _ = build_settings(tree, out)
    for key, value in {'Enabled': 1, 'ControlMode': 1, 'LocalEnabled': 1, 'SnowEnabled': 1,
                       'PrecipType': 1, 'PersistentWetEnabled': 1, 'AdvancedShelterEnabled': 1}.items():
        native.fixture_weather_set(('AlchemyWeather' + key).encode(), value)
    native.fixture_weather_step(1.)
    def counts():
        result = (C.c_double * 4)(); native.fixture_weather_counts(result); return tuple(result)
    for state in ((1, 1, 1, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 1, 1, 1)):
        native.fixture_weather_lifecycle(*state)
        before = counts()
        for frame in range(5): native.fixture_weather_step(2. + frame / 60.)
        assert counts() == before, 'disconnected/teleport idle queries terrain or refreshes UI'
    native.fixture_weather_lifecycle(0, 1, 1, 0)
    native.fixture_weather_step(10.)
    assert counts()[1] > before[1], 'weather fails to resume after arrival'
    # Execute each exact production entry guard with poisoned scene services
    # behind it. The remaining render body is deliberately unreachable here;
    # the GL suites exercise those bodies with a live world.
    pipe = tree.read('indra/newview/pipeline.cpp')
    guards = []
    for name in ('renderWeather', 'renderWeatherParticles', 'generateWeatherSurfaceCover',
                 'generateWeatherAdvancedShelter', 'bindWeatherMaterial'):
        body = function(pipe, 'void LLPipeline::' + name + '(')
        opening = body.index('{')
        prefix = body[opening + 1:body.index('if (gDisconnected', opening)]
        assert not re.search(r'\bgAgent\.|LLWorld::getInstance', prefix), 'scene read before guard: ' + name
        guard = function(body, 'if (gDisconnected')
        guards.append('void ' + name + '(){' + guard + '\nthrow std::runtime_error("scene accessed");}\n')
    code = r'''
#include <stdexcept>
bool gDisconnected=false,world=true,region=true;int teleport=0,suspensions=0;
struct LLWorld {static bool instanceExists(){return world;}};
struct LLAgent {enum {TELEPORT_NONE=0};};
struct Agent {void* getRegion(){return region?this:nullptr;}int getTeleportState(){return teleport;}}gAgent;
struct ALPanelWeatherSettings {static void suspendLibraryWeather(){++suspensions;}};
struct LLShaderMgr {enum {WEATHER_MATERIAL_ENABLED};};
struct Shader {int enabled=1;void uniform1i(int,int v){enabled=v;}} shaderObject,*shader=&shaderObject;
struct Exposure {void reset(){}} exposure,*auxiliary_exposure=&exposure;
GUARDS
extern "C" __declspec(dllexport) int check(int disconnected,int exists,int hasRegion,int moving){
 gDisconnected=disconnected;world=exists;region=hasRegion;teleport=moving;suspensions=0;shader->enabled=1;
 renderWeather();renderWeatherParticles();generateWeatherSurfaceCover();generateWeatherAdvancedShelter();bindWeatherMaterial();
 return suspensions==5 && shader->enabled==0;
}
'''.replace('GUARDS', ''.join(guards))
    path = out / 'guards.cpp'; path.write_text(code, encoding='utf-8')
    guards_lib = C.CDLL(str(compiler_command(out, [path], 'guards', dll=True)))
    guards_lib.check.argtypes = [C.c_int] * 4
    for state in ((1, 1, 1, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 1, 1, 1)):
        assert guards_lib.check(*state), 'unsafe entry guard: ' + str(state)
    agent = tree.read('indra/newview/llagent.cpp')
    assert 'suspendLibraryWeather();' in function(agent, 'void LLAgent::setTeleportState('), 'missing teleport start hook'
    region = function(agent, 'void LLAgent::setRegion(')
    assert 'rebaseLibraryWeather(origin_shift);' in region, 'plain crossing still fully suspends weather'
    start = region.index('        if (!mRegionp ||')
    hook = region[start:region.index('        LL_INFOS', start)]
    code = r'''
bool gDisconnected=false;int moving=0,suspends=0,rebases=0;
struct LLVector3 {double x=0,y=0,z=0;void setVec(const LLVector3& v){*this=v;}} lastShift;
LLVector3 operator-(const LLVector3& a,const LLVector3& b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
struct Region {LLVector3 origin;LLVector3 getOriginGlobal(){return origin;}};
struct ALPanelWeatherSettings {
 static void suspendLibraryWeather(){++suspends;}
 static void rebaseLibraryWeather(const LLVector3& v){++rebases;lastShift=v;}
};
struct Agent {
 enum {TELEPORT_NONE=0};Region* mRegionp=nullptr;int getTeleportState(){return moving;}
 void changed(Region* regionp){HOOK}
};
extern "C" __declspec(dllexport) int check(int oldRegion,int teleport,int disconnected){
 Region source{{256,256,0}},destination{{512,0,0}};Agent agent;agent.mRegionp=oldRegion?&source:nullptr;
 moving=teleport;gDisconnected=disconnected;suspends=rebases=0;agent.changed(&destination);
 const bool full=!oldRegion || teleport || disconnected;
 return suspends==int(full) && rebases==int(!full) &&
  (full || (lastShift.x==256 && lastShift.y==-256 && lastShift.z==0));
}
'''.replace('HOOK', hook)
    path = out / 'region.cpp'; path.write_text(code, encoding='utf-8')
    region_lib = C.CDLL(str(compiler_command(out, [path], 'region', dll=True)))
    region_lib.check.argtypes = [C.c_int] * 3
    for state in ((1, 0, 0), (0, 0, 0), (1, 1, 0), (1, 0, 1)):
        assert region_lib.check(*state), 'wrong crossing/login/teleport/disconnect hook: ' + str(state)
    print('PASS production setRegion branch: crossing rebases only; login/teleport/disconnect fully suspend')
    print('PASS active weather -> disconnect/no world/no region/teleport: five render guards + production idle; arrival resumes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-ref', default='WORKTREE')
    parser.add_argument('--check', choices=('ranges', 'lifecycle'))
    args = parser.parse_args(); tree = SourceTree(args.source_ref)
    out = ROOT / '.tmp/weather_port/inworld_fix2' / args.source_ref
    if args.check in (None, 'ranges'): ranges(tree, out / 'ranges')
    if args.check in (None, 'lifecycle'): lifecycle(tree, out / 'lifecycle')


if __name__ == '__main__': main()
