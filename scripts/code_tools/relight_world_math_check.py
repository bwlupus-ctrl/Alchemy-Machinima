"""Compile/run production world maths tests 37..41 without a viewer build.

python scripts/code_tools/relight_world_math_check.py [--out DIR]
The assertions are extracted verbatim from alrelightmath_test.cpp. The small TUT
adapter only reports failures and provides test_group/ensure; no maths is copied.
"""
import argparse
import os
from pathlib import Path
import subprocess

REPO = Path(__file__).resolve().parents[2]


def check(out):
    out = Path(out).resolve(); out.mkdir(parents=True,exist_ok=True)
    test = (REPO / 'indra/newview/tests/alrelightmath_test.cpp').read_text()
    tests = test[test.index('// ---- [P1-lite] world cube shadow maps'):test.rindex('}')]
    shim = '''#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    cpp = out / 'world_math.cpp'
    cpp.write_text(shim+tests+'\n}\nint main() { tut::relight_object o; o.test<37>(); o.test<38>(); o.test<39>(); o.test<40>(); o.test<41>(); std::puts("[world-math] PASS tests 37..41"); }\n')
    vswhere = Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere),'-latest','-property','installationPath'],text=True).strip()
    cmd = out / 'world_math.cmd'; exe = out / 'world_math.exe'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc /I "%s" "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\n' %
                   (Path(vs)/'VC/Auxiliary/Build/vcvars64.bat',REPO/'indra/newview',cpp,exe,out/'world_math.obj',exe))
    result = subprocess.run([str(cmd)],cwd=out,shell=True,capture_output=True,text=True)
    assert result.returncode == 0, result.stdout+result.stderr
    print(result.stdout.strip(),flush=True)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out',default='.tmp/relight_world_math')
    check(ap.parse_args().out)
