"""WP-C fixes: compile production signal registration, scene loops and seed wiring.

Viewer services/widgets are adapters; callbacks are reached only by settings
signals or editor onCommit, never by directly invoking a production handler.
--reference reads a commit without changing HEAD, to demonstrate red-before.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

from sensor_director_check import ROOT, function


def check(reference=None):
    def read(name):
        path = 'indra/newview/' + name
        if reference:
            return subprocess.check_output(['git', 'show', reference + ':' + path],
                                           cwd=ROOT, text=True, encoding='utf-8', timeout=20)
        return (ROOT / path).read_text(encoding='utf-8')

    out = ROOT / '.tmp/iso_sensor/wpc/restore' / ('reference' if reference else 'current')
    out.mkdir(parents=True, exist_ok=True)
    control = read('llviewercontrol.cpp')
    listener = re.search(r'    setting_setup_signal_listener\(gSavedSettings, "CineSensorPreset",.*?\n    \}\);',
                         function(control, 'void settings_setup_listeners()'), re.S)
    listener = listener.group() if listener else ''
    page = read('alpanelexposurepage.cpp')
    line_commit = function((ROOT / 'indra/llui/lllineeditor.cpp').read_text(encoding='utf-8'),
                           'void LLLineEditor::onCommit()')
    seed = page[page.index('    LL', page.index('"CineSensorISO", control->getValue()')):
                page.index('    mCustomSensorCard =')]
    # Keep the actual widget lookup, prevalidator and commit registration.
    seed = re.sub(r'    ALKitBindingRegistry::instance\(\).add\(this, "sensor_seed",.*?;\n', '', seed, flags=re.S)
    digit_class = re.search(r'class SensorSeedDigits.*?\n\};', page, re.S)
    director = read('llfloaterdirector.cpp')
    scene_list = function(director, 'const std::vector<std::string>& LLFloaterDirector::sceneSettingsList()')
    save_start = director.index('    LLSD settings = LLSD::emptyMap();', director.index('LLSD LLFloaterDirector::'))
    save = director[save_start:director.index('    scene["projector_shafts"]', save_start)]
    load = function(director, 'bool LLFloaterDirector::loadScene(')
    load_loop = function(load, 'if (scene["settings"].isMap())')
    scene_guard = 'ALScopedPresetMaterializing preset_guard;' if 'ALScopedPresetMaterializing preset_guard;' in load[:load.index(load_loop)] else ''
    bank = read('aldiopterpresetbank.cpp')
    guard = '\n'.join(function(bank, s) for s in (
        'bool alDiopterIsPresetMaterializing()', 'ALScopedPresetMaterializing::ALScopedPresetMaterializing()',
        'ALScopedPresetMaterializing::~ALScopedPresetMaterializing()'))
    history = read('alkithistory.cpp')
    replay = '\n'.join(function(history, s) for s in (
        'ALKitHistory::ScopedReplay::ScopedReplay()', 'ALKitHistory::ScopedReplay::~ScopedReplay()',
        'bool ALKitHistory::replaying()'))
    restore_sources = {
        'settings_import': (read('llappviewer.cpp'), 'bool LLAppViewer::loadSettingsFromDirectory(', 'loadFromFile('),
        'graphics_import': (read('llpresetsmanager.cpp'), 'void LLPresetsManager::loadPreset(', 'loadFromFile('),
        'selftest': (read('aluiperfselftest.cpp'), None, None),
        'legacy_undo': (read('alsettingsundohistory.cpp'), None, None),
        'kit_preset': (read('alkitpresets.cpp'), 'void apply(const std::vector<std::string>& settings, const LLSD& values)', 'control->set('),
    }
    restore_methods = []
    for name, (source, signature, write) in restore_sources.items():
        if signature:
            scope = function(source, signature)
            guarded = 'ALScopedPresetMaterializing preset_guard;' in scope[:scope.index(write)]
        elif name == 'selftest':
            scope = source[source.index('ALScopedPresetMaterializing preset_guard;'):]
            guarded = scope.index('CineSensorPreset') < scope.index('control->setValue(')
        else:
            scope = function(source, 'void replayModel(')
            guarded = 'ALKitHistory::ScopedReplay replay;' in scope[:scope.index('auto apply_change')]
        declaration = 'ALKitHistory::ScopedReplay replay;' if name == 'legacy_undo' else 'ALScopedPresetMaterializing preset_guard;'
        restore_methods.append('void ' + name + '(bool iso_first){' +
                               (declaration if guarded else '') + 'applyPair(iso_first);}')
    settings_xml = read('app_settings/settings_alchemy.xml')
    expected = sorted(set(re.findall(r'<key>(CineSensor\w+)</key>', settings_xml)) -
                      {'CineSensorPhysicalOrderEnabled', 'CineSensorNoiseEnabled'})
    prefix = r'''
#include "alsensormodel.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using F32=float; using F64=double; using U32=unsigned; using S32=int; using LLWString=std::u32string;
template<class T> T llmin(T a,T b){return std::min(a,b);}
template<class T> T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
struct LLSD {
    using Real=double; double value=0; std::string text; bool string=false,map=false;
    std::map<std::string,LLSD> entries;
    LLSD()=default; LLSD(double v):value(v){} LLSD(int v):value(v){}
    LLSD(const std::string& v):text(v),string(true){}
    static LLSD emptyMap(){LLSD v;v.map=true;return v;}
    LLSD& operator[](const std::string& key){map=true;return entries[key];}
    bool isMap()const{return map;} bool has(const std::string& key)const{return entries.count(key)!=0;}
    double asReal()const{return string?std::strtod(text.c_str(),nullptr):value;}
    int asInteger()const{return static_cast<int>(value);}
    std::string asString()const{if(string)return text;std::ostringstream s;s<<value;return s.str();}
};
struct Signal {
    std::vector<std::function<void()>> callbacks;
    void fire(){for(const auto& cb:callbacks)cb();}
};
struct LLControlVariable {
    LLSD value; Signal signal;
    LLSD getValue()const{return value;}
    void setValue(const LLSD& v){bool changed=value.asString()!=v.asString();value=v;if(changed)signal.fire();}
};
struct Settings {
    std::map<std::string,LLControlVariable> values;
    LLControlVariable* getControl(const std::string& key){return &values[key];}
    F32 getF32(const std::string& key){return static_cast<F32>(values[key].value.asReal());}
    U32 getU32(const std::string& key){return static_cast<U32>(values[key].value.asReal());}
    void setF32(const std::string& key,F32 v){values[key].setValue(LLSD(v));}
    void setU32(const std::string& key,U32 v){values[key].setValue(LLSD(static_cast<double>(v)));}
} gSavedSettings;
template<class F> void setting_setup_signal_listener(Settings& s,const std::string& key,F cb)
{s.getControl(key)->signal.callbacks.emplace_back(cb);}
static bool sPresetMaterializing=false;
class ALScopedPresetMaterializing {bool mPrior;public:ALScopedPresetMaterializing();~ALScopedPresetMaterializing();};
static int sKitHistoryReplay=0;
struct ALKitHistory {struct ScopedReplay {ScopedReplay();~ScopedReplay();};static bool replaying();};
namespace LLTextValidate {
struct ValidatorImpl {virtual bool validate(const std::string&)=0;virtual bool validate(const LLWString&)=0;};
struct Validator {ValidatorImpl* impl;explicit Validator(ValidatorImpl& v):impl(&v){};};
}
struct LLUICtrl {
    LLSD value; std::vector<std::function<void(LLUICtrl*,const LLSD&)>> commits;
    LLSD getValue()const{return value;} void setValue(const LLSD& v){value=v;}
    template<class F> void setCommitCallback(F cb){commits.emplace_back(cb);}
    template<class F> void setValidateCallback(F){} // LLLineEditor::onCommit does not call this.
    void onCommit(){for(const auto& cb:commits)cb(this,value);}
    LLUICtrl* getParent(){return this;}
};
struct LLLineEditor:LLUICtrl {
    LLTextValidate::ValidatorImpl* validator=nullptr;
    bool mSelectAllonCommit=true;
    void updateHistory(){} void setControlValue(const LLSD&){} void resetDirty(){} void selectAll(){}
    void onCommit();
    void setPrevalidate(LLTextValidate::Validator v){validator=v.impl;}
    bool type(const std::string& text){if(validator&&!validator->validate(text))return false;value=LLSD(text);return true;}
};
void alKitSetTypedControl(Settings& s,const std::string& key,const LLSD& v){s.setU32(key,static_cast<U32>(v.asReal()));}
namespace ALCineLightRigPersistence {int legacyFXForStorage(int v){return v;}}
namespace ALProjectorShaftPresets {std::vector<std::string> trackedSettings(){return {};}}
namespace ALRotoInk {const std::vector<std::string>& settings(){static std::vector<std::string> v;return v;}}
namespace ALTron {const std::vector<std::string>& settings(){static std::vector<std::string> v;return v;}}
struct LLFloaterDirector {static const std::vector<std::string>& sceneSettingsList();};
std::vector<std::string>& sceneSettingsList(){static auto v=LLFloaterDirector::sceneSettingsList();return v;}
struct Page {
    LLLineEditor editor; LLUICtrl* mSensorSeed=nullptr; LLUICtrl* mSensorSeedRow=nullptr;
    template<class T> T* getChild(const std::string&){return &editor;}
    void build();
};
'''
    sensor_header = read('alsensorsettings.h').replace('#include "llviewercontrol.h"', '')
    validation = function(page, 'bool sensorSeedTextValid(') + '\n' + function(page, 'std::string sensorSeedText(')
    main = r'''
int main(int argc,char** argv){
    const std::string mode=argv[1];
    settings_setup_listeners(); // No Page has been constructed.
    if(mode=="startup"){
        for(U32 preset:{1u,2u,0u,3u}){
            gSavedSettings.setF32("CineSensorCustomBaseISO",250.f);
            gSavedSettings.setF32("CineSensorISO",12345.f);
            gSavedSettings.setU32("CineSensorPreset",preset);
            if(gSavedSettings.getF32("CineSensorISO")!=ALSensorSettings::preset().baseISO)return 2;
        }
    }else if(mode=="scene"){
        std::vector<std::string> keys=EXPECTED_KEYS;
        const std::map<std::string,F32> custom={
            {"CineSensorCustomBaseISO",250.f},{"CineSensorCustomSwitchISO",1600.f},{"CineSensorCustomHeadroom",4.25f},
            {"CineSensorCustomLCGFullWell",36000.f},{"CineSensorCustomLCGReadNoise",2.5f},
            {"CineSensorCustomLCGAnalogBase",.4f},{"CineSensorCustomLCGPedestal",256.f},{"CineSensorCustomLCGWhite",16383.f},
            {"CineSensorCustomHCGFullWell",9000.f},{"CineSensorCustomHCGReadNoise",1.2f},
            {"CineSensorCustomHCGAnalogBase",1.6f},{"CineSensorCustomHCGPedestal",128.f},{"CineSensorCustomHCGWhite",8191.f}};
        for(const auto& key:keys){
            if(std::count(sceneSettingsList().begin(),sceneSettingsList().end(),key)!=1)return 2;
            gSavedSettings.setF32(key,custom.count(key)?custom.at(key):1.f);
        }
        gSavedSettings.setF32("CineSensorNoiseAmount",1.25f);gSavedSettings.setF32("CineSensorChroma",.5f);
        for(const auto& key:sceneSettingsList())if(key=="CineSensorNoiseEnabled"||key=="CineSensorPhysicalOrderEnabled")return 2;
        gSavedSettings.setU32("CineSensorPreset",1);gSavedSettings.setF32("CineSensorISO",3200.f);
        gSavedSettings.setU32("CineSensorSeed",4294967295u);
        const auto saved=saveScene();
        for(const auto& key:keys)gSavedSettings.setF32(key,0.f);
        loadScene(saved);
        for(const auto& key:keys)if(gSavedSettings.getControl(key)->getValue().asString()!=saved.entries.at("settings").entries.at(key).asString())return 2;
        // Missing sensor keys keep all current sensor values, including preset and ISO.
        LLSD old=LLSD::emptyMap();old["settings"]=LLSD::emptyMap();loadScene(old);
        for(const auto& key:keys)if(gSavedSettings.getControl(key)->getValue().asString()!=saved.entries.at("settings").entries.at(key).asString())return 2;
        // Explicitly exercise both list orders with the production load loop.
        for(bool iso_first:{false,true}){
            auto& order=sceneSettingsList();
            std::stable_sort(order.begin(),order.end(),[iso_first](const auto& a,const auto& b){
                return a==(iso_first?"CineSensorISO":"CineSensorPreset") && b!=a;});
            gSavedSettings.setU32("CineSensorPreset",2);loadScene(saved);
            if(gSavedSettings.getF32("CineSensorISO")!=3200.f)return 2;
        }
    }else if(mode=="restore"){
        for(auto restore:{settings_import,graphics_import,selftest,legacy_undo,kit_preset,kit_undo})
            for(bool iso_first:{false,true}){
                gSavedSettings.setU32("CineSensorPreset",2);gSavedSettings.setF32("CineSensorISO",50.f);
                restore(iso_first);
                if(gSavedSettings.getF32("CineSensorISO")!=3200.f)return 2;
            }
        {ALScopedPresetMaterializing outer;{ALScopedPresetMaterializing inner;}applyPair(true);
            if(gSavedSettings.getF32("CineSensorISO")!=3200.f)return 2;}
        gSavedSettings.setU32("CineSensorPreset",2);
        if(gSavedSettings.getF32("CineSensorISO")!=ALSensorSettings::preset().baseISO)return 2;
    }else if(mode=="seed"){
        Page page;page.build();
        for(const auto& text:{"123abc","","4294967296","0","4294967295"}){
            gSavedSettings.setU32("CineSensorSeed",16777217u);
            page.editor.setValue(LLSD(std::string(text)));page.editor.onCommit();
            const U32 expected=std::string(text)=="0"?0u:std::string(text)=="4294967295"?4294967295u:16777217u;
            if(gSavedSettings.getU32("CineSensorSeed")!=expected||page.editor.getValue().asString()!=std::to_string(expected))return 2;
        }
        if(page.editor.type("123abc")||page.editor.type("-1")||page.editor.type("1.5"))return 2;
        if(!page.editor.type("")||!page.editor.type("0")||!page.editor.type("4294967295"))return 2;
        if(!page.editor.validator->validate(LLWString(U"4294967295"))||page.editor.validator->validate(LLWString(U"123abc")))return 2;
    }else return 3;
    std::cout<<"PASS: "<<mode<<'\n';
}
'''.replace('EXPECTED_KEYS', '{' + ','.join('"' + k + '"' for k in expected) + '}')
    source = '\n'.join((prefix, line_commit, sensor_header, guard, replay, validation,
                        digit_class.group() if digit_class else '',
                        'void settings_setup_listeners(){' + listener + '}', scene_list,
                        'LLSD saveScene(){LLSD scene;' + save + 'return scene;}',
                        'void loadScene(LLSD scene){' + scene_guard + load_loop + '}',
                        'void Page::build(){' + seed + '}',
                        'void applyPair(bool iso_first){if(iso_first)gSavedSettings.setF32("CineSensorISO",3200.f);'
                        'gSavedSettings.setU32("CineSensorPreset",1);if(!iso_first)gSavedSettings.setF32("CineSensorISO",3200.f);}',
                        '\n'.join(restore_methods),
                        'void kit_undo(bool iso_first){ALKitHistory::ScopedReplay replay;applyPair(iso_first);}', main))
    cpp = out / 'restore.cpp'
    cpp.write_text(source, encoding='utf-8')
    (out / 'linden_common.h').write_text('', encoding='utf-8')
    vc = sorted((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    exe = out / 'restore.exe'
    cmd = out / 'build.cmd'
    cmd.write_text('@echo off\ncall "' + str(vc) + '" >nul 2>&1\n'
                   'cl /nologo /std:c++17 /EHsc /O2 /I"' + str(out) + '" /I"' + str(ROOT / 'indra/newview') + '" "' +
                   str(cpp) + '" "' + str(ROOT / 'indra/newview/alsensormodel.cpp') + '" /Fe:"' + str(exe) +
                   '" /Fo"' + out.as_posix() + '/"\nexit /b %ERRORLEVEL%\n', encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=ROOT, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr
    failed = []
    for case in ('startup', 'scene', 'restore', 'seed'):
        result = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=20)
        print(('PASS' if result.returncode == 0 else 'FAIL') + ': ' + case, flush=True)
        if result.returncode:
            failed.append(case)
    assert not failed, 'regressions: ' + ', '.join(failed)
    mutations = {
        'startup_registration': ('void settings_setup_listeners(){' + listener + '}',
                                 'void settings_setup_listeners(){}', 'startup'),
        'scene_restore_guard': ('void loadScene(LLSD scene){ALScopedPresetMaterializing preset_guard;',
                                'void loadScene(LLSD scene){', 'scene'),
        'settings_import_guard': ('void settings_import(bool iso_first){ALScopedPresetMaterializing preset_guard;',
                                  'void settings_import(bool iso_first){', 'restore'),
        'seed_commit_validation': ('if (sensorSeedTextValid(control->getValue().asString()))',
                                   'if (true)', 'seed'),
        'seed_digit_filter': ('seed_editor->setPrevalidate(LLTextValidate::Validator(seed_digits));', '', 'seed'),
    }
    for name, (original, replacement, case) in mutations.items():
        assert source.count(original) == 1, name + ': mutation seam drift'
        cpp.write_text(source.replace(original, replacement), encoding='utf-8')
        result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=ROOT, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, name + ': compile failure does not kill a mutation\n' + result.stdout + result.stderr
        result = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=20)
        assert result.returncode == 2, name + ': escaped mutation'
        print('PASS: rejected wiring mutation ' + name, flush=True)
    cpp.write_text(source, encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=ROOT, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference')
    args = parser.parse_args()
    try:
        check(args.reference)
    except (AssertionError, ValueError) as exc:
        print('FAIL:', exc)
        sys.exit(1)
