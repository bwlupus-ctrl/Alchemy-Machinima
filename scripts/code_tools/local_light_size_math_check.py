#!/usr/bin/env python3
"""Compile/run the TUT math checks standalone and the actual C++ radius adapter.

No viewer target, HEAD/worktree movement, GL, or proprietary references.
Uses the installed MSVC development environment. Outputs stay under .tmp/lls_cpu.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    out = ROOT / ".tmp/lls_cpu"
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / "indra/newview/alenvintensity.cpp").read_text(encoding="utf-8")
    function = re.search(r"    F32 localLightSize\([^)]*\)\s*\{.*?\n    \}", source, re.S)
    assert function, "production localLightSize adapter missing"
    shim = r'''
#include <algorithm>
#include <cmath>
#include <cassert>
#include <iostream>
#include <limits>
using F32=float;
struct Controls { float size=0; bool includeRig=false; } gSavedSettings;
struct LLViewerObject { bool rig=false; bool isCineRigEmitter()const{return rig;} };
template<class T> struct LLCachedControl {
    LLCachedControl(Controls&,const char*,T){}
    operator T()const{return T(gSavedSettings.size);}
};
template<class T> T llclamp(T value,T lo,T hi){return std::clamp(value,lo,hi);}
bool neutral=false;
bool isNeutral(){return neutral;}
'''
    shim += function[0]
    shim += r'''
int main(){
    LLViewerObject world,rig;rig.rig=true;
    for(bool include:{false,true}) {
        gSavedSettings.includeRig=include;
        for(float value:{0.f,.05f,.15f,.3f,.5f}) {
            gSavedSettings.size=value;
            assert(localLightSize(&world)==value);
            assert(localLightSize(&rig)==0.f);
            assert(localLightSize(nullptr)==0.f);
            neutral=true;assert(localLightSize(&world)==0.f);neutral=false;
        }
    }
    gSavedSettings.size=-1;assert(localLightSize(&world)==0.f);
    gSavedSettings.size=2;assert(localLightSize(&world)==.5f);
    gSavedSettings.size=std::numeric_limits<float>::quiet_NaN();assert(localLightSize(&world)==0.f);
    gSavedSettings.size=std::numeric_limits<float>::infinity();assert(localLightSize(&world)==0.f);
    std::cout<<"PASS actual C++ radius adapter: runtime, clamps, rig exclusion, neutral preview\n";
}
'''
    (out / "radius_adapter.cpp").write_text(shim, encoding="utf-8")
    devcmd = Path("C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat")
    assert devcmd.exists(), "MSVC development environment missing"
    batch = fr'''@echo off
call "{devcmd}" -arch=x64 -host_arch=x64 >nul
cl /nologo /std:c++17 /EHsc /O2 /DAL_LOCAL_LIGHT_SIZE_STANDALONE indra\newview\tests\allocallightsize_test.cpp /Fe.tmp\lls_cpu\math.exe /Fo.tmp\lls_cpu\math.obj
if errorlevel 1 exit /b %errorlevel%
.tmp\lls_cpu\math.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /O2 .tmp\lls_cpu\radius_adapter.cpp /Fe.tmp\lls_cpu\adapter.exe /Fo.tmp\lls_cpu\adapter.obj
if errorlevel 1 exit /b %errorlevel%
.tmp\lls_cpu\adapter.exe
'''
    command = out / "run.cmd"
    command.write_text(batch, encoding="utf-8")
    result = subprocess.run(["cmd", "/d", "/c", str(command)], cwd=ROOT, timeout=90,
                            capture_output=True, text=True)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
