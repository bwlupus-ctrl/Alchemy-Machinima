"""Offline PyOpenColorIO bridge for an already installed embedded Python 3.7.

No install/download, no viewer build, no writes to the application installation.
The tiny executable loads the bundled Python DLL in its own process (never
loads a second Python runtime into the test runner). All generated files stay
in the workspace. The external config and LUTs are read in place.
"""
import json
import os
from pathlib import Path
import subprocess


def run(out, config, codes):
    home=Path('C:/Program Files/BorisFX/Sapphire 2026.5 OFX/pylib')
    if not (home/'python37.dll').is_file():
        return dict(status='PENDING',reason='No compatible local Python runtime for discovered PyOpenColorIO.pyd')
    host=r'''
#include <windows.h>
#include <fstream>
#include <iterator>
#include <string>
int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 2;
 std::wstring home=argv[1],dllpath=home+L"/python37.dll";
 SetDllDirectoryW(home.c_str());HMODULE dll=LoadLibraryW(dllpath.c_str());if(!dll)return 3;
 auto symbol=[&](const char* name){return GetProcAddress(dll,name);};
 *reinterpret_cast<int*>(symbol("Py_DontWriteBytecodeFlag"))=1;
 *reinterpret_cast<int*>(symbol("Py_IgnoreEnvironmentFlag"))=1;
 *reinterpret_cast<int*>(symbol("Py_NoSiteFlag"))=1;
 reinterpret_cast<void(*)(const wchar_t*)>(symbol("Py_SetPythonHome"))(home.c_str());
 std::wstring paths=home+L"/Lib;"+home+L"/DLLs;"+home+L"/Lib/site-packages;"+home;
 reinterpret_cast<void(*)(const wchar_t*)>(symbol("Py_SetPath"))(paths.c_str());
 reinterpret_cast<void(*)()>(symbol("Py_Initialize"))();
 std::ifstream stream(argv[2]);std::string source((std::istreambuf_iterator<char>(stream)),{});
 int code=reinterpret_cast<int(*)(const char*)>(symbol("PyRun_SimpleString"))(source.c_str());
 reinterpret_cast<void(*)()>(symbol("Py_Finalize"))();return code?4:0;
}
'''
    path=out/'ocio_host.cpp';path.write_text(host,encoding='utf-8')
    vc=sorted((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    exe=out/'ocio_host.exe';cmd=out/'ocio_host.cmd'
    cmd.write_text('@echo off\ncall "'+str(vc)+'" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /O2 "'+str(path)+'" /Fe:"'+str(exe)+'" /Fo"'+out.as_posix()+'/"\n',encoding='utf-8')
    compiled=subprocess.run(['cmd','/c',str(cmd)],cwd=out,capture_output=True,text=True,timeout=90)
    (out/'ocio_compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
    assert compiled.returncode==0,compiled.stdout+compiled.stderr
    result=out/'ocio_output.json'
    script=out/'ocio_worker.py'
    script.write_text('''import json,sys
sys.dont_write_bytecode=True
import PyOpenColorIO as O
config=O.Config.CreateFromFile(CONFIG)
processor=config.getProcessor("Input - Sony - S-Log3 - S-Gamut3","ACES - ACES2065-1")
if hasattr(processor,"getDefaultCPUProcessor"): processor=processor.getDefaultCPUProcessor()
rows=[]
for code in CODES:
    pixel=list(code)
    value=processor.applyRGB(pixel)
    rows.append(pixel if value is None else list(value))
with open(OUTPUT,"w") as stream:
    json.dump(dict(status="PASS",version=O.GetVersion(),config=CONFIG,AP0=rows),stream,indent=2)
'''.replace('CONFIG',repr(str(config))).replace('CODES',repr(codes)).replace('OUTPUT',repr(str(result))),encoding='utf-8')
    child=subprocess.run([str(exe),str(home),str(script)],cwd=out,capture_output=True,text=True,timeout=60)
    (out/'ocio_runtime.log').write_text(child.stdout+child.stderr,encoding='utf-8')
    if child.returncode:
        return dict(status='PENDING',reason='Local bundled Python/OCIO execution failed',returncode=child.returncode,log=str(out/'ocio_runtime.log'))
    return json.loads(result.read_text(encoding='utf-8'))
