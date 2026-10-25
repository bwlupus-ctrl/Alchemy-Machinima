"""Compile/run production world maths tests 37..43 and 46 without a viewer build.

python scripts/code_tools/relight_world_math_check.py [--out DIR]
The assertions are extracted verbatim from alrelightmath_test.cpp. The small TUT
adapter only reports failures and provides test_group/ensure; no maths is copied.
"""
import argparse
from collections import Counter
import os
from pathlib import Path
import re
import subprocess

REPO = Path(__file__).resolve().parents[2]


def check(out):
    out = Path(out).resolve(); out.mkdir(parents=True,exist_ok=True)
    test = (REPO / 'indra/newview/tests/alrelightmath_test.cpp').read_text()
    numbers = list(map(int,re.findall(r'relight_object::test<(\d+)>\s*\(',test)))
    duplicates = [n for n,count in Counter(numbers).items() if count > 1]
    assert not duplicates, f'duplicate TUT specializations: {duplicates}'
    tut = (REPO / 'indra/externals/tut/tut/tut.hpp').read_text()
    maximum = int(re.search(r'int MaxTestsInGroup\s*=\s*(\d+)',tut)[1])
    assert max(numbers) <= maximum, f'test {max(numbers)} exceeds TUT group limit {maximum}'
    print(f'[world-math] unique TUT tests 1..{max(numbers)}; group limit {maximum}',flush=True)
    tests = test[test.index('// ---- [P1-lite] world cube shadow maps'):test.rindex('}')]
    shim = '''#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <vector>
#include "alareashadowmath.h"
#include "alworldshadowmath.h"
namespace tut {
void ensure(const char* m, bool c) { if (!c) { std::fprintf(stderr,"FAIL: %s\\n",m); std::exit(1); } }
template<class A,class B> void ensure_equals(const char* m,const A& a,const B& b) { ensure(m,a==b); }
struct relight_data {};
template<class T> struct test_group { struct object : T { template<int N> void test(); }; };
using relight_object = test_group<relight_data>::object;
void close(const char* m,double a,double b,double tol=1.e-10) { ensure(m,std::isfinite(a)&&std::abs(a-b)<=tol); }
'''
    # Execute the actual scene-load settings loop and migration with tiny value/control adapters.
    # Load every sparse combination after non-default live values, including an absent settings map.
    director = (REPO / 'indra/newview/llfloaterdirector.cpp').read_text()
    # End at the LOOK reset loop itself; later scene migrations have their own
    # dependencies and must not accidentally become part of this small adapter.
    look_reset = director.index('    for (const char* setting : {"RenderAreaShadowStrength"')
    look_end = director.index('\n    }\n', look_reset) + len('\n    }\n')
    load = director[director.index('    // only apply keys this floater owns:'):look_end]
    scene_shim = r'''#include <map>
#include <string>
namespace scene_regression {
struct Value {
    bool map = true;
    std::map<std::string,double> values;
    bool isMap() const { return map; }
    bool has(const std::string& key) const { return values.count(key) != 0; }
    double operator[](const std::string& key) const { return values.at(key); }
};
struct Scene { Value settings; Value& operator[](const char*) { return settings; } };
struct LLControlVariable {
    double value, identity;
    void setValue(double v) { value = v; }
    void resetToDefault(bool) { value = identity; }
};
struct Controls {
    std::map<std::string,LLControlVariable> values;
    LLControlVariable* getControl(const std::string& key) { return &values.at(key); }
};
const std::vector<std::string>& sceneSettingsList() {
    static const std::vector<std::string> names = {"RenderAreaShadowStrength", "RenderAreaShadowContrast",
        "RenderAreaShadowWorldScreenReach", "RenderAreaShadowWorldAmount", "RenderAreaShadowScreenAmount"};
    return names;
}
void check() {
    for (unsigned mask = 0; mask < 33; ++mask) {
        Controls gSavedSettings;
        Scene scene;
        scene.settings.map = mask != 32;
        unsigned i = 0;
        for (const auto& key : sceneSettingsList()) {
            gSavedSettings.values[key] = {.37, i == 2 ? 0. : 1.};
            if (mask != 32 && (mask & (1u << i))) scene.settings.values[key] = .6;
            ++i;
        }
''' + load + r'''
        i = 0;
        for (const auto& key : sceneSettingsList()) {
            const double expected = mask != 32 && (mask & (1u << i)) ? .6 : (i == 2 ? 0. : 1.);
            tut::ensure("old/sparse scene resets each missing look key, preserves every authored key",gSavedSettings.getControl(key)->value == expected);
            ++i;
        }
    }
    std::puts("[scene-load] PASS 32 sparse maps + missing settings after non-default values");
}
}
'''
    cpp = out / 'world_math.cpp'
    cpp.write_text(shim+tests+'\n}\n'+scene_shim+'\nint main() { scene_regression::check(); tut::relight_object o; o.test<37>(); o.test<38>(); o.test<39>(); o.test<40>(); o.test<41>(); o.test<42>(); o.test<43>(); o.test<46>(); std::puts("[world-math] PASS tests 37..43 + 46"); }\n')
    vswhere = Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere),'-latest','-property','installationPath'],text=True).strip()
    cmd = out / 'world_math.cmd'; exe = out / 'world_math.exe'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc /I "%s" "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\n' %
                   (Path(vs)/'VC/Auxiliary/Build/vcvars64.bat',REPO/'indra/newview',cpp,exe,out/'world_math.obj',exe))
    result = subprocess.run([str(cmd)],cwd=out,shell=True,capture_output=True,text=True,timeout=120)
    assert result.returncode == 0, result.stdout+result.stderr
    print(result.stdout.strip(),flush=True)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out',default='.tmp/relight_world_math')
    check(ap.parse_args().out)
