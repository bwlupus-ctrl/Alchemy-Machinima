#!/usr/bin/env python3
"""Compile/run only the arbiter fixture with mocked viewer objects (not a viewer build)."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/avatar_anim_speed_fixture'


def main():
    OUT.mkdir(exist_ok=True, parents=True)
    nv = ROOT / 'indra/newview'
    implementation = '\n'.join(re.sub(r'^#include .*$', '', (nv / name).read_text(encoding='utf-8'),
        flags=re.M) for name in ['alavataranimspeed.h', 'alavataranimspeed.cpp',
                                 'aldanceballmodel.h', 'aldanceballmodel.cpp'])
    fixture = Path(__file__).with_suffix('.cpp').read_text(encoding='utf-8')
    source = OUT / 'fixture.cpp'
    source.write_text(fixture.replace('// PRODUCTION_IMPLEMENTATION', implementation), encoding='utf-8')
    env = os.environ.copy()
    compiler = shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')
    if compiler:
        exe = OUT / ('fixture.exe' if os.name == 'nt' else 'fixture')
        command = [compiler, '-std=c++17', str(source), '-o', str(exe)]
    elif os.name == 'nt':
        vs = Path('C:/Program Files/Microsoft Visual Studio/18/Community')
        setup = vs / 'VC/Auxiliary/Build/vcvars64.bat'
        batch = OUT / 'environment.cmd'
        batch.write_text(f'@call "{setup}" >nul\n@if errorlevel 1 exit /b 1\n@set\n', encoding='utf-8')
        result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)],
            timeout=30, capture_output=True, text=True, check=True)
        for line in result.stdout.splitlines():
            if '=' in line:
                key, value = line.split('=', 1)
                if key: env[key] = value
        compiler = shutil.which('cl.exe', path=env.get('Path', env.get('PATH')))
        if not compiler: raise RuntimeError('No C++ compiler found')
        exe = OUT / 'fixture.exe'
        command = [compiler, '/nologo', '/std:c++17', '/EHsc', str(source), '/Fe:' + str(exe), '/Fo:' + str(OUT / 'fixture.obj')]
    else:
        raise RuntimeError('No C++ compiler found')
    subprocess.run(command, env=env, timeout=60, check=True, cwd=OUT)
    subprocess.run([str(exe)], timeout=10, check=True)


if __name__ == '__main__':
    main()
