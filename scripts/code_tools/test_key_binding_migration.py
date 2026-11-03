#!/usr/bin/env python3
"""Run the production in-memory migration rule with small parameter/parser doubles.

Run from a VS developer shell. Only the extracted pure rule is compiled; no
viewer build or user settings are involved. Products stay under .tmp.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def build_source():
    source = (ROOT / 'indra/newview/llviewerinput.cpp').read_text()
    start = source.index('void migrateBindingMode(')
    end = source.index('\n}\n}', start) + 2
    rule = source[start:end]
    return r'''
#include <algorithm>
#include <cassert>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>
using S32 = int;
using KEY = int;
using MASK = unsigned;
constexpr KEY KEY_NONE = 0;
constexpr MASK MASK_NONE = 0;
struct LLKeyboard {
    static bool keyFromString(const std::string& s, KEY* key) {
        *key = s == "F9" ? 1009 : s.size() == 1 ? s[0] : KEY_NONE;
        return *key != KEY_NONE;
    }
    static bool maskFromString(const std::string& s, MASK* mask) {
        if (s == "NONE") *mask = 0;
        else if (s == "CTL") *mask = 1;
        else if (s == "ALT") *mask = 2;
        else if (s == "CTL_ALT") *mask = 3;
        else if (s == "SHIFT") *mask = 4;
        else return false;
        return true;
    }
};
struct LLViewerInput {
    struct Command {
        std::string value;
        Command(const char* s) : value(s) {}
        const std::string& getValue() const { return value; }
    };
    struct KeyBinding { std::string key, mask; Command command; };
    struct Bindings : std::vector<KeyBinding> {
        void add(const KeyBinding& binding) { push_back(binding); }
    };
    struct KeyMode { Bindings bindings; };
};
''' + rule + r'''
int main() {
    using Mode = LLViewerInput::KeyMode;
    Mode blinkDefaults;
    blinkDefaults.bindings.add({"F9", "NONE", "pov_eye_blink"});
    for (int marker : {0, 1, 2, 3, 4}) {
        Mode user;
        migrateBindingMode(user, blinkDefaults, marker, 1);
        assert(user.bindings.size() == (marker < 3 ? 1u : 0u));
    }
    Mode customBlink;
    customBlink.bindings.add({"F9", "SHIFT", "custom_command"});
    migrateBindingMode(customBlink, blinkDefaults, 2, 1);
    assert(customBlink.bindings.size() == 1); // any occupied F9 mask blocks migration
    Mode clearedBlink;
    clearedBlink.bindings.add({"", "NONE", "pov_eye_blink"});
    migrateBindingMode(clearedBlink, blinkDefaults, 2, 1);
    assert(clearedBlink.bindings.size() == 1); // explicit command clear survives
    Mode hotDefaults;
    for(int i=1;i<=4;++i) hotDefaults.bindings.add({std::to_string(i), "CTL_ALT", LLViewerInput::Command(("director_hot_gaze_"+std::to_string(i)).c_str())});
    for(int version:{0,1,2,3,4}) {
        Mode user; migrateBindingMode(user,hotDefaults,version,1);
        assert(user.bindings.size()==(version<4?4u:0u));
    }
    Mode customHot;
    customHot.bindings.add({"2","SHIFT","custom_command"});
    customHot.bindings.add({"","NONE","director_hot_gaze_3"});
    migrateBindingMode(customHot,hotDefaults,2,1);
    assert(customHot.bindings.size()==4); // add only 1 and 4; occupied/cleared survive
    Mode keyDefaults;
    keyDefaults.bindings.add({"K", "CTL_ALT", "toggle_actor_key"});
    for (int marker : {0, 1, 2, 3, 4, 5}) {
        Mode user; migrateBindingMode(user, keyDefaults, marker, 1);
        assert(user.bindings.size() == (marker < 5 ? 1u : 0u));
    }
    Mode occupiedKey;
    occupiedKey.bindings.add({"K", "SHIFT", "custom_command"});
    migrateBindingMode(occupiedKey, keyDefaults, 4, 1);
    assert(occupiedKey.bindings.size() == 1);
    Mode clearedKey;
    clearedKey.bindings.add({"", "NONE", "toggle_actor_key"});
    migrateBindingMode(clearedKey, keyDefaults, 4, 1);
    assert(clearedKey.bindings.size() == 1);
    Mode defaults;
    defaults.bindings.add({"V", "CTL_ALT", "toggle_viewfinder"});
    defaults.bindings.add({"W", "NONE", "push_forward"});
    for (int version : {0, 1, 2, 3}) {
        Mode user;
        migrateBindingMode(user, defaults, version, 1);
        assert(user.bindings.size() == (version < 2 ? 1u : 0u));
        if (version < 2) assert(user.bindings[0].command.getValue() == "toggle_viewfinder");
    }
    for (const char* mask : {"NONE", "ALT", "CTL", "CTL_ALT", "SHIFT", "CTL_SHIFT", "ALT_SHIFT", "CTL_ALT_SHIFT", "INVALID"}) {
        Mode user;
        user.bindings.add({"V", mask, "custom_command"});
        migrateBindingMode(user, defaults, 1, 1);
        assert(user.bindings.size() == 1);
        assert(user.bindings[0].command.getValue() == "custom_command");
        assert(user.bindings[0].mask == mask);
    }
    for (const char* key : {"", "B"}) {
        Mode user;
        user.bindings.add({key, "NONE", "toggle_viewfinder"});
        migrateBindingMode(user, defaults, 0, 1);
        assert(user.bindings.size() == 1);
        assert(user.bindings[0].key == key);
    }
    // This is the original file's runtime binding count, not this mode's size.
    for (int marker : {0, 1, 2, 3}) {
        Mode empty_file;
        migrateBindingMode(empty_file, defaults, marker, 0);
        assert(empty_file.bindings.empty());
    }
    for (int marker : {2, 3}) {
        Mode saved;
        saved.bindings.add({"W", "NONE", "push_forward"});
        migrateBindingMode(saved, defaults, marker, 1);
        assert(saved.bindings.size() == 1); // cleared Viewfinder remains absent
    }
    Mode tracked, additions;
    tracked.bindings.add({"W", "NONE", "push_forward"});
    migrateBindingMode(tracked, defaults, 0, 1, &additions);
    assert(tracked.bindings.size() == 2);
    assert(additions.bindings.size() == 1);
    assert(additions.bindings[0].command.getValue() == "toggle_viewfinder");
    Mode other_key;
    other_key.bindings.add({"B", "SHIFT", "custom_command"});
    migrateBindingMode(other_key, defaults, 1, 1);
    assert(other_key.bindings.size() == 2);
    assert(other_key.bindings[1].command.getValue() == "toggle_viewfinder");
    Mode user;
    migrateBindingMode(user, defaults, 0, 1);
    migrateBindingMode(user, defaults, 0, 1);
    assert(user.bindings.size() == 1); // repeat loads do not duplicate
    Mode no_default;
    Mode absent;
    migrateBindingMode(absent, no_default, 0, 1);
    assert(absent.bindings.empty());
    std::cout << "PASS: free default, any V mask blocks, other keys, zero-binding files, persisted markers, runtime additions, preserved custom/cleared commands, unrelated omissions, idempotence\n";
}
'''


def check_legacy_detection():
    """Require HEAD's entire legacy upgrade and Controls save, plus the marker."""
    def head(path):
        return subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=ROOT, text=True)
    path = 'indra/newview/llviewerinput.cpp'
    source = (ROOT / path).read_text()
    original = head(path)
    def loader(text):
        start = text.index('S32 LLViewerInput::loadBindingsXML(')
        return text[start:text.index('\nS32 count_masks(', start)]
    current_loader = loader(source)
    migration_start = current_loader.index("    // HEAD's fallback decision")
    assert current_loader == loader(original)  # Hot Gaze changes only the existing shared migration rule
    assert current_loader.index('// fix version') < migration_start
    constructor = 'LLViewerInput::Keys::Keys()'
    def keys_constructor(text):
        begin = text.index(constructor)
        return text[begin:text.index('void LLViewerInput::resetBindings()', begin)]
    assert keys_constructor(source) == keys_constructor(original)
    shared_loader = source[source.index('void LLViewerInput::migrateKeysXML('):source.index('S32 LLViewerInput::loadBindingsXML(')]
    assert 'xml_version' not in shared_loader
    assert 'binding_count == 0 || migration_version >= 5' in shared_loader
    assert 'getS32("KeyBindingsMigrationVersion")' in shared_loader
    header = 'indra/newview/llviewerinput.h'
    version = 'const S32 keybindings_xml_version = 1;'
    assert version in (ROOT / header).read_text() and version in head(header)
    path = 'indra/newview/llkeyconflict.cpp'
    conflict = (ROOT / path).read_text()
    def save(text):
        begin = text.index('void LLKeyConflictHandler::saveToSettings(')
        return text[begin:text.index('\nvoid ', begin + 10)]
    assert 'LLViewerInput::loadKeysXML(filename, keys)' in save(conflict)
    assert 'parser.readXUI(filename, keys)' not in save(conflict)
    assert save(conflict).index('LLViewerInput::loadKeysXML(filename, keys)') < save(conflict).index('switch (mLoadMode)')
    shipped = 'indra/newview/app_settings/key_bindings.xml'
    import xml.etree.ElementTree as ET
    assert ET.fromstring((ROOT / shipped).read_text()).attrib == ET.fromstring(head(shipped)).attrib
    for mode in ET.parse(ROOT / shipped).getroot():
        expected = [('pov_eye_blink', 'F9', 'NONE'), ('toggle_actor_key', 'K', 'CTL_ALT')] + [
            ('director_hot_gaze_' + str(i), str(i), 'CTL_ALT') for i in range(1, 5)]
        for command, key, mask in expected:
            bindings = [b for b in mode if b.get('command') == command]
            assert len(bindings) == 1 and bindings[0].get('key') == key and bindings[0].get('mask') == mask
    settings = ET.parse(ROOT / 'indra/newview/app_settings/settings.xml').getroot().find('map')
    entries = list(settings)
    setting = entries[next(i for i, e in enumerate(entries) if e.tag == 'key' and e.text == 'KeyBindingsMigrationVersion') + 1]
    values = list(setting)
    fields = {values[i].text: values[i + 1].text for i in range(0, len(values), 2)}
    assert fields['Persist'] == '1' and fields['Type'] == 'S32' and fields['Value'] == '0'
    assert conflict.count('setS32("KeyBindingsMigrationVersion", 5)') == 1
    print('PASS: legacy loader/file format preserved; Controls save migrates all modes before splicing the edited mode and advancing marker 5')


def roundtrip_source():
    from relight_area_policy_check import method
    input_source = ROOT / 'indra/newview/llviewerinput.cpp'
    save = method(ROOT / 'indra/newview/llkeyconflict.cpp', 'void LLKeyConflictHandler::saveToSettings(')
    splice = save[save.index('            switch (mLoadMode)'):save.index('            keys.xml_version.set')]
    migration = method(input_source, 'void LLViewerInput::migrateKeysXML(')
    load = method(input_source, 'bool LLViewerInput::loadKeysXML(')
    preamble = build_source().split('int main()')[0]
    preamble = preamble.replace('struct Bindings : std::vector<KeyBinding> {', 'struct Bindings : std::vector<KeyBinding> { void set(const Bindings& b,bool){*this=b;}')
    preamble = preamble.replace('struct KeyMode { Bindings bindings; };', 'struct KeyMode { Bindings bindings; bool isProvided()const{return true;} }; struct Keys {KeyMode first_person,third_person,sitting,edit_avatar;bool validateBlock()const{return true;}}; static void migrateKeysXML(const std::string&,Keys&,S32,Keys* = nullptr);static bool loadKeysXML(const std::string&,Keys&);int loadBindingMode(KeyMode& m,int){return int(m.bindings.size());}')
    return preamble + r'''
#include <map>
#include <sstream>
constexpr int MODE_FIRST_PERSON=0,MODE_THIRD_PERSON=1,MODE_SITTING=2,MODE_EDIT_AVATAR=3,LL_PATH_APP_SETTINGS=0;
struct Settings{int marker=0;int getS32(const char*){return marker;}void setS32(const char*,int v){marker=v;}}gSavedSettings;
struct Directory{std::string getExpandedFilename(int,const char*){return "defaults";}}directory,*gDirUtilp=&directory;
// A parser boundary double actually serializes and reparses every mode. It
// deliberately keeps no migrated runtime state between save and reload.
std::map<std::string,std::string> files;
void write(const std::string& path,const LLViewerInput::Keys& keys){std::ostringstream s;
 for(auto* m:{&keys.first_person,&keys.third_person,&keys.sitting,&keys.edit_avatar}){
 s<<m->bindings.size()<<"\n";for(auto& b:m->bindings)s<<b.key<<" "<<b.mask<<" "<<b.command.getValue()<<"\n";}
 files[path]=s.str();}
struct LLSimpleXUIParser{bool readXUI(const std::string& path,LLViewerInput::Keys& keys){if(!files.count(path))return false;std::istringstream s(files[path]);
 for(auto* m:{&keys.first_person,&keys.third_person,&keys.sitting,&keys.edit_avatar}){int n;s>>n;for(int i=0;i<n;++i){std::string k,mask,c;s>>k>>mask>>c;m->bindings.add({k,mask,LLViewerInput::Command(c.c_str())});}}return true;}};
#define LL_ERRS() std::cerr
#define LL_ENDL std::endl
''' + migration + '\n' + load + r'''
void saveMode(int mLoadMode,const LLViewerInput::KeyMode& mode){LLViewerInput::Keys keys;std::string filename="user";
''' + 'if (' + ('LLViewerInput::loadKeysXML(filename, keys)' if 'LLViewerInput::loadKeysXML(filename, keys)' in save else 'false') + ') {\n' + splice + r'''
 write("user",keys);
''' + next(line.strip() for line in save.splitlines() if 'setS32("KeyBindingsMigrationVersion"' in line) + r'''
}}
int main(){using Input=LLViewerInput;Input::Keys defaults;
 for(auto* m:{&defaults.first_person,&defaults.third_person,&defaults.sitting,&defaults.edit_avatar}){
 m->bindings.add({"F9","NONE","pov_eye_blink"});m->bindings.add({"V","CTL_ALT","toggle_viewfinder"});m->bindings.add({"K","CTL_ALT","toggle_actor_key"});for(int i=1;i<=4;++i)m->bindings.add({std::to_string(i),"CTL_ALT",Input::Command(("director_hot_gaze_"+std::to_string(i)).c_str())});}
 write("defaults",defaults);
 for(int marker:{0,1,2,3,4})for(int edited:{0,1,2,3}){
 gSavedSettings.marker=marker;Input::Keys raw;for(auto* m:{&raw.first_person,&raw.third_person,&raw.sitting,&raw.edit_avatar}){m->bindings.add({"W","NONE","push_forward"});if(marker>=3)m->bindings.add({"F9","NONE","pov_eye_blink"});if(marker>=4)for(int i=1;i<=4;++i)m->bindings.add({std::to_string(i),"CTL_ALT",Input::Command(("director_hot_gaze_"+std::to_string(i)).c_str())});}write("user",raw);
 Input::Keys runtime;assert(Input::loadKeysXML("user",runtime));
 auto* mode=edited==0?&runtime.first_person:edited==1?&runtime.third_person:edited==2?&runtime.sitting:&runtime.edit_avatar;
 mode->bindings.add({"B","SHIFT","edited_command"});saveMode(edited,*mode);
 assert(gSavedSettings.marker==5);Input::Keys reloaded;assert(Input::loadKeysXML("user",reloaded));
 int n=0;for(auto* m:{&reloaded.first_person,&reloaded.third_person,&reloaded.sitting,&reloaded.edit_avatar}){
 for(int i=1;i<=4;++i){const auto command="director_hot_gaze_"+std::to_string(i);assert(std::count_if(m->bindings.begin(),m->bindings.end(),[&](const auto& b){return b.command.getValue()==command;})==1);}
 assert(std::count_if(m->bindings.begin(),m->bindings.end(),[](const auto& b){return b.command.getValue()=="pov_eye_blink" && b.key=="F9" && b.mask=="NONE";})==1);
 assert(std::count_if(m->bindings.begin(),m->bindings.end(),[](const auto& b){return b.command.getValue()=="toggle_actor_key" && b.key=="K" && b.mask=="CTL_ALT";})==1);
 assert(m->bindings.size()==size_t(7+(marker<2?1:0)+(n++==edited?1:0)));}
 }
 std::cout<<"PASS: production load/migrate + Controls mode splice + serialized save/reload, edit each mode at markers 0..4: defaults survive in all four modes\n";
}
'''


def main():
    from native_check_tools import compile_run, check_directory
    check_legacy_detection()
    directory = check_directory('key_migration')
    for name, source in [('rules', build_source()), ('roundtrip', roundtrip_source())]:
        result = compile_run(directory, source, name, ['/W4', '/WX'])
        assert result.returncode == 0, result.stdout + result.stderr
        print(result.stdout.strip())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
