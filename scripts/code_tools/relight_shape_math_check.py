"""Run production light-shape TUT assertions without building the viewer.

--math-ref HEAD runs the new assertions against the pre-fix production header.
Output and compiler artifacts stay in this worktree's .tmp/light_shape directory.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def check(math_ref=None):
    out = ROOT / '.tmp/light_shape' / ('math_before' if math_ref else 'math_after')
    out.mkdir(parents=True, exist_ok=True)
    header = Path('indra/newview/alrelightmath.h')
    text = (subprocess.check_output(['git', 'show', math_ref+':'+header.as_posix()], cwd=ROOT, text=True)
            if math_ref else (ROOT/header).read_text())
    (out/'alrelightmath.h').write_text(text)
    tests = (ROOT/'indra/newview/tests/alrelightmath_test.cpp').read_text()
    numbers = [*range(1, 10), 44, 45]
    bodies = []
    for number in numbers:
        match = re.search(r'template<> template<> void relight_object::test<'+str(number)+r'>\(\)', tests)
        start = tests.index('{', match.end())
        end, depth = start+1, 1
        while depth:
            depth += (tests[end] == '{') - (tests[end] == '}')
            end += 1
        bodies.append(tests[match.start():end])
    shim = r'''
#include "alrelightmath.h"
#include <limits>
#include <cstdio>
namespace tut {
int failures=0;
void ensure(const char* message,bool ok) {
    if (!ok && failures++ < 20) std::printf("FAIL: %s\n",message);
}
struct relight_data {};
template<class T> struct test_group { struct object : T { template<int N> void test(); }; };
using relight_object = test_group<relight_data>::object;
void close(const char* message,double a,double b,double tolerance=1.e-10) {
    ensure(message,std::isfinite(a)&&std::abs(a-b)<=tolerance);
}
'''
    source = shim+'\n'.join(bodies)+'\n}\nint main() { tut::relight_object o;\n'
    source += ''.join(f'o.test<{n}>();\n' for n in numbers)
    source += '''for (double r : {0.0,1.0}) {
    double gain=ALRelight::gain(.75,1.5,.5,r,.1);
    std::printf("[shape-math] radius=%.1f gain=%.9f receiver_1.2m=%.9f\\n",r,gain,ALRelight::capped(1.2,1.5,.5,r,.1,gain,4));
}
std::printf("[shape-math] %s: %d failures\\n",tut::failures ? "FAIL" : "PASS",tut::failures); return tut::failures ? 1 : 0; }\n'''
    (out/'shape_math.cpp').write_text(source)
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere), '-latest', '-property', 'installationPath'], text=True).strip()
    exe = out/'shape_math.exe'
    cmd = out/'compile.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++17 /EHsc "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\n' %
                   (Path(vs)/'VC/Auxiliary/Build/vcvars64.bat', out/'shape_math.cpp', exe, out/'shape_math.obj', exe))
    return subprocess.call(['cmd', '/d', '/c', str(cmd)], cwd=out)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--math-ref')
    raise SystemExit(check(parser.parse_args().math_ref))
