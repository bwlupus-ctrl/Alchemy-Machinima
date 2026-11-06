"""Repository-relative native-check products and installed MSVC discovery."""
from pathlib import Path
import os
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def check_directory(name):
    directory = ROOT / '.tmp' / 'native_checks' / name
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def vcvars64():
    locator = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
    installation = subprocess.check_output([str(locator), '-latest', '-products', '*',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property', 'installationPath'], text=True).strip()
    script = Path(installation) / 'VC/Auxiliary/Build/vcvars64.bat'
    if not installation or not script.is_file():
        raise RuntimeError('MSVC x64 tools were not found by vswhere')
    return script


def compile_run(directory, source, name='test', flags=(), libraries=(), run=True):
    path = directory / (name + '.cpp')
    path.write_text(source, encoding='utf-8')
    exe = directory / (name + '.exe')
    command = directory / (name + '.cmd')
    response = directory / (name + '.rsp')
    response.write_text('/nologo /std:c++20 /EHsc ' + ' '.join(flags)
        + ' /Fo"' + str(directory / (name + '.obj')) + '" /Fe"' + str(exe)
        + '" "' + str(path) + '" ' + ' '.join(libraries), encoding='utf-8')
    command.write_text('@echo off\ncall "' + str(vcvars64()) + '" >nul\n'
        + 'cl @"' + str(response) + '"\nexit /b %ERRORLEVEL%\n', encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(command)], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    if run:
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True)
    return exe


def viewer_native_flags():
    """Use an existing checkout's CMake libraries; never build viewer targets."""
    common = Path(subprocess.check_output(['git', 'rev-parse', '--git-common-dir'], cwd=ROOT, text=True).strip()).resolve().parent
    candidates = sorted(set(ROOT.glob('build-*/newview/alchemy-bin.vcxproj')) |
                        set(common.glob('build-*/newview/alchemy-bin.vcxproj')))
    for project in reversed(candidates):
        build = project.parent.parent
        if (build / 'llcommon/Release/llcommon.lib').is_file():
            break
    else:
        raise RuntimeError('Native shader diagnostics prerequisites missing: build Release llcommon and llmath '
                           'and install the CMake/vcpkg dependencies first. Searched checkout/build-* and '
                           'git-common checkout/build-* for newview/alchemy-bin.vcxproj with '
                           'llcommon/Release/llcommon.lib. This gate cannot run or pass without native libraries.')
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    tree = ET.parse(project)
    group = next(n for n in tree.findall('m:ItemDefinitionGroup', ns) if "Release|x64" in n.get('Condition', ''))
    defines = group.find('m:ClCompile/m:PreprocessorDefinitions', ns).text.split(';')
    flags = ['/MT', '/O2', '/utf-8', '/DNDEBUG', '/DBOOST_ALL_NO_LIB'] + [f'/D"{d}"' for d in defines if d and not d.startswith('%')]
    includes = list((ROOT / 'indra').iterdir()) + [build / 'newview'] + list((build / 'vcpkg_installed').glob('*/include'))
    includes += [d / 'tracy' for d in includes if d.name == 'include']
    flags += [f'/I"{d}"' for d in includes if d.is_dir()]
    libraries = [f'"{build / name / "Release" / (name + ".lib")}"' for name in ('llcommon', 'llmath')]
    required = ('apr-1', 'aprapp-1', 'libexpatMT', 'boost_', 'fmt', 'xxhash', 'simdjson', 'simdutf', 'TracyClient', 'zs')
    installed = list((build / 'vcpkg_installed').glob('*/lib/*.lib'))
    missing = [str(build / name / 'Release' / (name + '.lib')) for name in ('llcommon', 'llmath')
               if not (build / name / 'Release' / (name + '.lib')).is_file()]
    missing += [prefix + '*.lib (vcpkg)' for prefix in required if not any(p.stem.startswith(prefix) for p in installed)]
    if missing:
        raise RuntimeError('Native shader diagnostics prerequisites missing: ' + ', '.join(missing)
                           + '. Build Release llcommon/llmath and install the CMake/vcpkg dependencies first; '
                           'this gate cannot run or pass without them.')
    libraries += [f'"{p}"' for p in installed if p.stem.startswith(required)]
    libraries += ['/link', '/OPT:REF', 'advapi32.lib', 'bcrypt.lib', 'user32.lib', 'shell32.lib', 'ws2_32.lib',
                  'dbghelp.lib', 'dbgeng.lib', 'winmm.lib', 'ole32.lib', 'version.lib', 'psapi.lib', 'synchronization.lib', 'iphlpapi.lib', 'rpcrt4.lib']
    return flags, libraries
