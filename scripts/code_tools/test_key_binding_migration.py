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
        *key = s.size() == 1 ? s[0] : KEY_NONE;
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
    assert current_loader[:migration_start] + '    return binding_count;\n}\n' == loader(original)
    assert current_loader.index('// fix version') < migration_start
    constructor = 'LLViewerInput::Keys::Keys()'
    def keys_constructor(text):
        begin = text.index(constructor)
        return text[begin:text.index('void LLViewerInput::resetBindings()', begin)]
    assert keys_constructor(source) == keys_constructor(original)
    shared_loader = source[source.index('void LLViewerInput::migrateKeysXML('):source.index('S32 LLViewerInput::loadBindingsXML(')]
    assert 'xml_version' not in shared_loader
    assert 'binding_count == 0 || migration_version >= 2' in shared_loader
    assert 'getS32("KeyBindingsMigrationVersion")' in shared_loader
    header = 'indra/newview/llviewerinput.h'
    version = 'const S32 keybindings_xml_version = 1;'
    assert version in (ROOT / header).read_text() and version in head(header)
    path = 'indra/newview/llkeyconflict.cpp'
    conflict = (ROOT / path).read_text()
    def save(text):
        begin = text.index('void LLKeyConflictHandler::saveToSettings(')
        return text[begin:text.index('\nvoid ', begin + 10)]
    marker = '''                    if (!temporary)
                    {
                        gSavedSettings.setS32("KeyBindingsMigrationVersion", 2);
                    }
'''
    assert marker in save(conflict)
    assert save(conflict).replace(marker, '') == save(head(path))
    shipped = 'indra/newview/app_settings/key_bindings.xml'
    import xml.etree.ElementTree as ET
    assert ET.fromstring((ROOT / shipped).read_text()).attrib == ET.fromstring(head(shipped)).attrib
    settings = ET.parse(ROOT / 'indra/newview/app_settings/settings.xml').getroot().find('map')
    entries = list(settings)
    setting = entries[next(i for i, e in enumerate(entries) if e.tag == 'key' and e.text == 'KeyBindingsMigrationVersion') + 1]
    values = list(setting)
    fields = {values[i].text: values[i + 1].text for i in range(0, len(values), 2)}
    assert fields['Persist'] == '1' and fields['Type'] == 'S32' and fields['Value'] == '0'
    assert conflict.count('setS32("KeyBindingsMigrationVersion", 2)') == 1
    print('PASS: git show HEAD legacy loader and Controls save identical apart from migration/marker; file format version unchanged; marker globally persisted')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='cl.exe')
    args = parser.parse_args()
    check_legacy_detection()
    with tempfile.TemporaryDirectory(prefix='key_migration_', dir=ROOT / '.tmp') as scratch:
        source = Path(scratch) / 'test.cpp'
        source.write_text(build_source(), encoding='utf-8')
        exe = Path(scratch) / 'test.exe'
        subprocess.run([args.compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                        str(source), '/Fe:' + str(exe),
                        '/Fo:' + str(Path(scratch) / 'test.obj')], check=True, cwd=scratch)
        return subprocess.run([str(exe)], check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
