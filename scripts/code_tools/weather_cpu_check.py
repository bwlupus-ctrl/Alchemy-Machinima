"""Compile/run production weather CPU sources and their TUT cases without a viewer build.

Uses real stdtypes.h and original tests; only the PCH and TUT assertion/registration
shells are replaced. No viewer algorithm is copied. Mutations compile in .tmp and
must fail the same tests. Native integration remains provisional until a viewer build.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ['slweather', 'alweatherclock', 'alweatherbridge', 'alweatheruimodel', 'alweathershelter', 'alweathermodel']


def compiler_command(out, sources, name, dll=False, includes=()):
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    output = out / (name + ('.dll' if dll else '.exe'))
    command = out / (name + '.cmd')
    # Everything is a known absolute path, never shell-expanded user input.
    args = ['cl', '/nologo', '/std:c++17', '/EHsc', '/O2', '/W3', '/DWIN32',
            *(['/LD'] if dll else []), *('/I"%s"' % p for p in [out, ROOT / 'indra/llcommon', ROOT / 'indra/newview', *includes]),
            *('"%s"' % p for p in sources), '/Fe:"%s"' % output, '/Fo"%s/"' % out]
    if dll: args += ['/link', '/IMPLIB:"%s"' % (out / (name + '.lib'))]
    command.write_text('@echo off\ncall "%s" >nul 2>&1\n%s\nexit /b %%ERRORLEVEL%%\n' % (vcvars, ' '.join(args)))
    result = subprocess.run(['cmd', '/d', '/c', str(command)], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr
    return output


def prepare(out):
    out.mkdir(parents=True, exist_ok=True)
    (out / 'linden_common.h').write_text('#pragma once\n#include "stdtypes.h"\n#include <string>\n#include <vector>\n#include <cmath>\n')
    (out / 'weather_tut.h').write_text('''#pragma once
#include <stdexcept>
#include <string>
#include <iostream>
namespace tut {
inline void ensure(const char* msg, bool yes) { if (!yes) throw std::runtime_error(msg); }
template<class A,class B> void ensure_equals(const char* msg,const A& a,const B& b) { ensure(msg,a==b); }
template<class Data> struct test_group {
  struct object { template<int N> void test(); };
  explicit test_group(const char*) {}
};
}
''')


def suite(out, mutation=None):
    prepare(out)
    sources = [ROOT / ('indra/newview/' + s + '.cpp') for s in SOURCES]
    if mutation:
        file, old, new = mutation
        text = (ROOT / ('indra/newview/' + file + '.cpp')).read_text(encoding='utf-8')
        assert old in text, old
        path = out / (file + '.cpp')
        path.write_text(text.replace(old, new, 1), encoding='utf-8')
        sources[SOURCES.index(file)] = path
    count = 0
    calls = []
    for name in SOURCES:
        text = (ROOT / ('indra/newview/tests/' + name + '_test.cpp')).read_text(encoding='utf-8')
        text = text.replace('#include "../test/lltut.h"', '#include "weather_tut.h"')
        text = text.replace('#include "../', '#include "')
        cases = re.findall(r'void\s+(\w+)::test<(\d+)>\(\)', text)
        assert cases, name
        count += len(cases)
        for obj, case in cases:
            calls.append('try { tut::%s{}.test<%s>(); } catch (const std::exception& e) { std::cerr << "%s:%s " << e.what() << "\\n"; ++failed; }' % (obj, case, name, case))
        text += '\nint run_%s() { int failed=0;\n%s\nreturn failed; }\n' % (name, '\n'.join(calls[-len(cases):]))
        path = out / (name + '_test.cpp'); path.write_text(text, encoding='utf-8'); sources.append(path)
    main = out / 'main.cpp'
    main.write_text('#include <iostream>\n' + '\n'.join('int run_%s();' % s for s in SOURCES) +
        '\nint main() { int failed=' + '+'.join('run_%s()' % s for s in SOURCES) +
        '; std::cout << "weather CPU: %d cases, " << failed << " failures\\n"; return failed ? 1 : 0; }' % count)
    sources.append(main)
    executable = compiler_command(out, sources, 'weather_cpu')
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=120)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    for name in SOURCES[:-1]:
        text = (ROOT / ('indra/newview/' + name + '.cpp')).read_text(encoding='utf-8')
        assert not re.search(r'(LLDate::now|LLFrameTimer::|std::chrono|time\s*\(|getTotalTime|gFrameCount)', text), name + ': direct clock'
    out = ROOT / '.tmp/weather_port/cpu'
    result = suite(out / 'baseline')
    print(result.stdout, end=''); print(result.stderr, end='')
    assert result.returncode == 0, 'CPU tests failed'
    if args.mutations:
        mutations = [
            ('snap', ('slweather', 'build <= melt && mState.snowDepth < 1e-4f', 'mState.snowDepth < 1e-4f')),
            ('rate', ('alweatherbridge', '* amount,', '* (1.f - amount),')),
            ('clock', ('alweatherclock', '++mTick;', 'mTick += 2;')),
            ('double-step', ('alweatherbridge', 'iteration == mIteration || ', '')),
            ('scheduler', ('alweatherbridge', 'lc.mRatePerMinute = 0.0;', 'lc.mRatePerMinute = 60.0;')),
            ('teleport', ('slweather', 'mPlaceSerial.fetch_add(1, std::memory_order_acq_rel);', '(void)mPlaceSerial;')),
            ('wet-fixed-step', ('slweather', 'mState.wetness + rate, 1.f', 'mState.wetness + rate * 2.f, 1.f')),
            ('empty-cover', ('alweathershelter', 'return batches ? Coverage::COMPLETE_COVERED : Coverage::COMPLETE_EMPTY;', 'return batches ? Coverage::COMPLETE_COVERED : Coverage::FAILED;')),
            ('probe-revision', ('alweathershelter', 'mPublished = signature;\n        mNext = seconds + 0.5;\n        return true;', 'return false;')),
            ('probe-rewind', ('alweathershelter', 'if (mValid && seconds < mLast) mNext = seconds;', '(void)mLast;')),
            ('strike-identity', ('alweatherbridge', 'visual_id ? visual_id : 1', 'strike.serial')),
        ]
        for name, mutation in mutations:
            result = suite(out / name, mutation)
            assert result.returncode != 0, 'SURVIVING MUTATION: ' + name
            print('mutation rejected:', name, result.stderr.strip().splitlines()[0])


if __name__ == '__main__':
    main()
