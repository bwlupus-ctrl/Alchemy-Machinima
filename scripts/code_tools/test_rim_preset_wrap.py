#!/usr/bin/env python3
"""Compile and run the production rim preset-apply path, without a viewer build.

Run from a VS developer shell: python scripts/code_tools/test_rim_preset_wrap.py
Tables, name dispatch, ownership selection, reset and apply are extracted verbatim
from production C++; only LLSD, control storage and the history scope use doubles.
Settings defaults/comments come from settings.xml. No preset-apply algorithm is
reimplemented in this test. Temporary compilation products stay under .tmp.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = ('Enabled', 'Strength', 'Width', 'Blur', 'Saturation', 'Clamp')
# Independent expectations, in the stable public preset order. Include all six
# settings so a swapped column or an omitted write cannot pass unnoticed.
EXPECTED = [
    ('Subtle Edge', True, .25, 1, .5, .5, 2),
    ('Soft Portrait Wrap', True, .65, 3, 1.5, .65, 4),
    ('Strong Backlight', True, .55, 2, .8, .7, 6),
    ('Hair Shine', True, .25, 1, .5, .7, 4),
    ('Silhouette Glow (any angle)', True, .8, 3.5, 1.8, .7, 6),
    ('Neon / Stage', True, .25, 1, .5, 1, 8),
    ('Dreamy Halo', True, 1.1, 4.5, 2.5, .6, 6),
    ('Noir Kicker', True, .25, 1, .5, .3, 2),
    ('Two-Sided Kick', True, .25, 1.5, .5, .7, 4),
    ('Fashion Edge', True, .35, 2, 1, .65, 4),
    ('Sci-Fi Hologram', True, .7, 2.5, 1.2, 1, 6),
    ('Candle Glow', True, .5, 2.5, 1.8, .8, 4),
    ('Metal Sheen', True, .25, 1, .5, .7, 4),
    ('Skin Sheen', True, .25, 2, 1.2, .55, 3),
    ('Soft Fill Wrap', True, .65, 3.5, 2, .65, 4),
    ('Horror Edge', True, .25, 1, .5, .4, 4),
    ('Moonlit Rim', True, .35, 2, 1.2, .5, 3),
    ('Glass Shell', True, .25, 1, .5, .5, 4),
    ('Tron Suit Kick', True, .25, 1, .5, 1, 6),
    ('Ares Red Backlight', True, .25, 1.5, .5, 1, 6),
    ('Golden Hour Flare', True, 1.4, 4.5, 2.2, .85, 8),
    ('Glacial Frost Edge', True, .25, 1, .5, .5, 2),
    ('Strobe Cut', True, .25, 1, .3, .7, 4),
    ('Aquatic Caustic Rim', True, .9, 3.2, 1.8, .9, 6),
    ('Ember Lick', True, .75, 2.8, 1.4, .8, 5),
    ('Overcast Diffusion', True, .6, 5, 2.5, .4, 2.5),
    ('Clamshell Beauty Edge', True, .5, 2, 1.2, .6, 3.5),
    ('Limelight Beam', True, .8, 2.2, 1, .75, 7),
    ('Crypt Pale Glow', True, .7, 3, 1.6, .2, 3),
    ('Rain Slick Glaze', True, .25, 1, .5, .6, 4),
    ('Dusk Horizon Kick', True, 1.1, 3.5, 1.5, .8, 6.5),
    ('Spectral Silhouette', True, 1.6, 6, 2.6, .3, 4.5),
    ('Natural Separation', True, .3, 1.5, 1, .6, 3),
    ('Dialogue Two-Shot', True, .3, 1.5, 1, .55, 3),
    ('Interview Hair Light', True, .35, 1.5, 1.2, .6, 3.5),
    ('Window Backlight', True, .5, 3, 1.6, .75, 5),
    ('Overcast Outdoor', True, .2, 2.5, 2, .4, 2),
    ('Night Street Edge', True, .3, 1.5, .8, .85, 5),
    ('Lamp Practical', True, .4, 2, 1.5, .8, 4),
    ('Dark Background Lift', True, .25, 1, .6, .5, 3),
]


def function(source, signature):
    """Take an entire definition unchanged; fail loudly if its signature drifts."""
    start = source.index(signature)
    brace = source.index('\n{', start) + 1
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


SHIM = r'''
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using F32 = float;
#define LL_ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
struct LLSD {
    double scalar = 0;
    std::map<std::string, LLSD> fields;
    LLSD() = default;
    LLSD(bool v) : scalar(v) {}
    LLSD(int v) : scalar(v) {}
    LLSD(double v) : scalar(v) {}
    static LLSD emptyMap() { return {}; }
    LLSD& operator[](const std::string& key) { return fields[key]; }
    const LLSD& operator[](const std::string& key) const { return fields.at(key); }
    bool has(const std::string& key) const { return fields.count(key) != 0; }
};
struct LLControlVariable {
    LLSD initial, value;
    std::string comment;
    void resetToDefault(bool) { value = initial; }
    void set(const LLSD& v) { value = v; }
    LLSD get() const { return value; }
    const std::string& getComment() const { return comment; }
};
struct LLControlGroup {
    struct ApplyFunctor {
        virtual ~ApplyFunctor() = default;
        virtual void apply(const std::string&, LLControlVariable*) = 0;
    };
    std::map<std::string, LLControlVariable> controls;
    LLControlVariable* getControl(const std::string& name) {
        auto it = controls.find(name);
        return it == controls.end() ? nullptr : &it->second;
    }
    void applyToAll(ApplyFunctor* f) {
        for (auto& kv : controls) f->apply(kv.first, &kv.second);
    }
    void setF32(const std::string& name, F32 value) { controls.at(name).set(LLSD(double(value))); }
    void setBOOL(const std::string& name, bool value) { controls.at(name).set(LLSD(value)); }
} gSavedSettings;
struct ALRigRimController {
    static const std::vector<std::string>& presetNames();
    static std::string presetDescription(const std::string&);
    static bool applyPresetByName(const std::string&);
};
namespace ALPresetLook {
    struct Load { Load() {} ~Load() {} };
    bool replaying() { return false; }
}
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
'''


def build_source():
    controller = (ROOT / 'indra/newview/alrigrimcontroller.cpp').read_text()
    loader = (ROOT / 'indra/newview/alkitpresets.cpp').read_text()
    header = (ROOT / 'indra/newview/alpresetlook.h').read_text()
    enum = re.search(r'enum Family \{.*?\};', header, re.S).group()
    ownership = '\n'.join(function(loader, sig) for sig in (
        'std::vector<std::string> select(',
        'const std::vector<std::string>& owned(',
        'void reset(', 'void apply(const std::vector<std::string>& settings,'))
    # This contiguous production slice includes BOTH tables and BOTH overloads.
    tables = controller[controller.index('struct RigRimPresetRow'):controller.index('} // namespace')]
    dispatch = '\n'.join(function(controller, sig) for sig in (
        'const std::vector<std::string>& ALRigRimController::presetNames()',
        'std::string ALRigRimController::presetDescription(',
        'bool ALRigRimController::applyPresetByName('))
    # Enforce explicit authorship as well as the resulting settings values.
    for table, field in (('RIG_RIM_PRESETS', 'mWrapValues'), ('RIG_RIM_FULL_PRESETS', 'mValues')):
        checks = f'for (const auto& row : {table}) {{\n'
        checks += 'std::map<std::string, int> counts;\n'
        checks += f'for (const auto& entry : row.{field}) ++counts[entry.first];\n'
        for suffix in SUFFIXES:
            key = 'CineRigRimWrap' + suffix
            checks += f'require(counts["{key}"] == 1, std::string(row.mName) + " explicit {key}");\n'
        checks += '}\n'
        if table == 'RIG_RIM_PRESETS':
            explicit_checks = checks
        else:
            explicit_checks += checks
    root_map = ET.parse(ROOT / 'indra/newview/app_settings/settings.xml').getroot().find('map')
    elements = list(root_map)
    settings = {}
    for key, spec in zip(elements[::2], elements[1::2]):
        if key.text.startswith('CineRigRim'):
            fields = list(spec)
            settings[key.text] = {k.text: v.text for k, v in zip(fields[::2], fields[1::2])}
    declarations = []
    for name, spec in settings.items():
        if not name.startswith('CineRigRim'):
            continue
        value = float(spec['Value'])
        declarations.append(f'gSavedSettings.controls[{json.dumps(name)}] = '
                            f'{{LLSD({float(value)}), LLSD({float(value)}), '
                            f'{json.dumps(spec["Comment"])}}};')
    expected = ',\n'.join('{' + json.dumps(row[0]) + ', {' +
                          ', '.join(str(float(v)) for v in row[1:]) + '}}'
                          for row in EXPECTED)
    keys = ', '.join(json.dumps('CineRigRimWrap' + s) for s in SUFFIXES)
    return (SHIM + '\nnamespace ALPresetLook {\n' + enum + '\n' + ownership + '\n}\n' +
            tables + dispatch + '\nint main() { try {\n' + '\n'.join(declarations) +
            '\n' + explicit_checks + r'''
struct Expected { const char* name; double values[6]; };
const Expected expected[] = {
''' + expected + r'''};
const char* keys[] = {''' + keys + r'''};
const auto& names = ALRigRimController::presetNames();
require(names.size() == LL_ARRAY_SIZE(expected), "public preset count");
const auto& owned = ALPresetLook::owned(ALPresetLook::RIM);
for (const char* key : keys)
    require(std::find(owned.begin(), owned.end(), key) != owned.end(), key);
for (size_t i = 0; i < names.size(); ++i) {
    require(names[i] == expected[i].name, "stable preset name/order");
    const std::string description = ALRigRimController::presetDescription(names[i]);
    require(!description.empty(), names[i] + " missing description");
    // Two dirty histories, plus every possible previous preset. This catches
    // missing writes, reset-to-default mistakes and cross-preset inheritance.
    for (int previous = -2; previous < int(names.size()); ++previous) {
        for (auto& kv : gSavedSettings.controls) kv.second.set(LLSD(previous == -2 ? 0.0 : 7.0));
        if (previous >= 0)
            require(ALRigRimController::applyPresetByName(names[previous]), "previous apply");
        std::map<std::string, double> excluded;
        for (const auto& kv : gSavedSettings.controls)
            if (std::find(owned.begin(), owned.end(), kv.first) == owned.end())
                excluded[kv.first] = kv.second.get().scalar;
        require(ALRigRimController::applyPresetByName(names[i]), names[i] + " apply failed");
        for (size_t k = 0; k < 6; ++k) {
            const double got = gSavedSettings.controls.at(keys[k]).get().scalar;
            require(std::abs(got - expected[i].values[k]) < 1.e-5, names[i] + " " + keys[k]);
        }
        require(gSavedSettings.controls.at(keys[0]).get().scalar == 1, names[i] + " wrap disabled");
        require(gSavedSettings.controls.at(keys[1]).get().scalar > 0, names[i] + " zero wrap");
        for (const auto& kv : excluded)
            require(gSavedSettings.controls.at(kv.first).get().scalar == kv.second,
                    names[i] + " changed excluded " + kv.first);
    }
}
const auto before = gSavedSettings.controls;
require(!ALRigRimController::applyPresetByName("__unknown_rim__"), "unknown preset accepted");
for (const auto& kv : before)
    require(gSavedSettings.controls.at(kv.first).get().scalar == kv.second.get().scalar,
            "unknown preset changed settings");
std::cout << "PASS: 40 presets, all six explicit wrap keys, 1680 production applies, excluded settings, unknown name\n";
return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='cl.exe')
    args = parser.parse_args()
    scratch_root = ROOT / '.tmp'
    scratch_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rim_wrap_', dir=scratch_root) as scratch:
        source = Path(scratch) / 'test.cpp'
        source.write_text(build_source(), encoding='utf-8')
        exe = Path(scratch) / 'test.exe'
        subprocess.run([args.compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                        str(source), '/Fe:' + str(exe),
                        '/Fo:' + str(Path(scratch) / 'test.obj')], check=True, cwd=scratch)
        return subprocess.run([str(exe)], check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
