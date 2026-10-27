#!/usr/bin/env python3
"""Run TUT against production factor functions, without building the viewer.

--source-ref HEAD is the fail-before control. Adapters replace viewer services,
not the exposure math. --dump emits production factors for the GL fixtures.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
# Baseline = the commit this feature was built on (pre Environment EV); not HEAD, which
# includes the feature once committed/merged. Override with env var.
import os as _os_base
BASE_REF = _os_base.environ.get('ENV_EV_MATH_CHECK_BASE', 'b8a911596c5')

ROOT = Path(__file__).resolve().parents[2]


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def compile_test(ref=None):
    out = ROOT / '.tmp/env_ev/cpu'
    out.mkdir(parents=True, exist_ok=True)
    def read(path):
        if ref:
            return subprocess.check_output(['git', 'show', f'{ref}:{path}'], cwd=ROOT, timeout=20).decode('utf-8')
        return (ROOT / path).read_text(encoding='utf-8')
    env = read('indra/newview/alenvintensity.cpp')
    header = read('indra/newview/alenvintensity.h')
    factors = header[header.index('struct Factors'):header.index('    // Effective factors')]
    shim = r'''
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <type_traits>
using F32=float;
struct LLColor3 { float mV[3]; LLColor3(float a=1,float b=1,float c=1):mV{a,b,c}{} };
struct LLColor4 { static LLColor4 white; }; LLColor4 LLColor4::white;
struct Controls {
    std::map<std::string,float> values;
    void setF32(const char* k,float v){values[k]=v;}
    void setBOOL(const char* k,bool v){values[k]=v;}
} gSavedSettings;
template<class T> struct LLCachedControl {
    std::string key; T fallback;
    LLCachedControl(Controls&,const char* k,T v):key(k),fallback(v){}
    operator T()const {
        if constexpr (std::is_arithmetic_v<T>) {
            auto it=gSavedSettings.values.find(key);
            return it==gSavedSettings.values.end()?fallback:T(it->second);
        } else return fallback;
    }
    T operator()()const{return T(*this);}
};
template<class T> T llclamp(T v,T lo,T hi){return std::clamp(v,lo,hi);}
template<class T> T llmin(T a,T b){return std::min(a,b);}
struct {float mGLVersion=4.6f;} gGLManager;
struct LLViewerObject {bool rig=false;bool isCineRigEmitter()const{return rig;}};
struct LLRender {static bool sClassicMode;}; bool LLRender::sClassicMode=false;
bool neutral=false,capture=false;
namespace ALEnvIntensity {
constexpr float EV_CLAMP=6,LIFT_EV_CLAMP=2,KELVIN_NEUTRAL=6500;
enum ETintSlot {TINT_SUN,TINT_MOON,TINT_AMBIENT};
bool isNeutral(){return neutral;} bool isSamplingCapture(){return capture;}
LLColor3 cachedTint(ETintSlot,float,LLColor4,float){return LLColor3();}
LLColor3 normMaxChannel(const LLColor3& c){return c;}
'''
    shim += factors + '\n' + function(env, 'inline F32 clampEV(') + '\n'
    for signature in ('bool envOnlyActive()', 'F32 environmentEV()', 'Factors compute(', 'F32 localLightEVScale(', 'F32 ssaoCeilingScale()'):
        if signature in env:
            shim += function(env, signature) + '\n'
    shim += '}\n'
    # This is the actual shared signature input for ordinary and cinematic
    # probes, including scenes with no local lights to invalidate them.
    probe = read('indra/newview/llreflectionmapmanager.cpp')
    start = probe.index('    st.mSunEV =', probe.index('ALProbeSched::EnvSample LLReflectionMapManager::gatherEnvSample()'))
    end = probe.index('    st.mLocalLightSize =', start)
    assignment = probe[start:end]
    marker = '    const F32 environment_ev = ALEnvIntensity::environmentEV();'
    if marker in probe[:start]:
        assignment = marker + '\n' + assignment
    shim += r'''
struct ProbeEnvironment { float mSunEV=0,mMoonEV=0,mLocalLightEV=0; };
ProbeEnvironment captureSignature() {
    static LLCachedControl<F32> sun_ev(gSavedSettings,"AlchemyEnvSunEV",0.f);
    static LLCachedControl<F32> moon_ev(gSavedSettings,"AlchemyEnvMoonEV",0.f);
    static LLCachedControl<F32> local_light_ev(gSavedSettings,"AlchemyEnvLocalLightEV",0.f);
    ProbeEnvironment st;
''' + assignment + '    return st;\n}\n'
    (out / 'env_ev_adapter.h').write_text(shim, encoding='utf-8')
    exe = out / ('before.exe' if ref else 'factors.exe')
    dev = 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat'
    cmd = out / 'run.cmd'
    cmd.write_text(f'@echo off\ncall "{dev}" -arch=x64 -host_arch=x64 >nul\n'
                   'cl /nologo /std:c++17 /EHsc /O2 /DAL_ENV_EV_STANDALONE '
                   '/I.tmp/env_ev/cpu /Iindra/externals/tut '
                   '/I../alchemy-machinima/build-Windows-vs2026-os/vcpkg_installed/x64-windows-alchemy/include '
                   'indra/newview/tests/alenvintensity_envev_test.cpp '
                   f'/Fe"{exe}" /Fo"{out / "factors.obj"}"\n', encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=ROOT, capture_output=True, text=True, timeout=90)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return exe


def fixture_factors(ref=None):
    exe = compile_test(ref)
    result = subprocess.run([str(exe), '--dump'], cwd=ROOT, capture_output=True, text=True, timeout=20, check=True)
    return json.loads(result.stdout)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-ref')
    ap.add_argument('--dump', action='store_true')
    ap.add_argument('--baseline', action='store_true', help='compare 2400 inactive/zero combinations bit-for-bit against HEAD')
    args = ap.parse_args()
    exe = compile_test(args.source_ref)
    if args.baseline:
        current = subprocess.check_output([str(exe), '--baseline'], cwd=ROOT, timeout=20)
        previous = subprocess.check_output([str(compile_test(BASE_REF)), '--baseline'], cwd=ROOT, timeout=20)
        assert current == previous, 'inactive/zero factors differ from HEAD'
        print('PASS 2400 inactive/zero factor combinations identical to HEAD (hex floats)')
        return 0
    result = subprocess.run([str(exe)] + (['--dump'] if args.dump else []), cwd=ROOT,
                            capture_output=True, text=True, timeout=20)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
