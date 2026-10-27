#!/usr/bin/env python3
"""Compile complete production listener registration against real llcontrol.

Only unrelated viewer callbacks and GL work are stubbed. The registration body,
signal helpers and shader-change handler are extracted intact, including lambdas
and direct getSignal connections. Existing viewer dependency libraries are read
from --dependency-build; no viewer target is built or modified.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def run(dependency_build):
    out = ROOT / '.tmp/env_only/signals'
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'indra/newview/llviewercontrol.cpp').read_text(encoding='utf-8')
    registration = function(source, 'void settings_setup_listeners()')
    # Compile every registration, including inline lambdas and direct connects.
    # Unknown dependencies fail compilation; never drop an unrecognised statement.
    callbacks = set(re.findall(r'setting_setup_signal_listener\(\s*\w+,\s*[^,;]+,\s*(\w+)\s*\);', registration))
    callbacks -= {'handleSetShaderChanged', 'handleRigRimWrapAmount'}
    stubs = '\n'.join(f'bool {name}(const LLSD&) {{ return true; }}' for name in sorted(callbacks))
    helper_start = source.index('void setting_setup_signal_listener(')
    helper_end = source.index('static void handleRigRimWrapAmount', helper_start)
    adapter = '#include "env_only_signal_stubs.h"\n' + stubs + '\n'
    adapter += 'namespace { U64 sShaderChangeCount = 0; }\n'
    adapter += function(source, 'U64 settings_shader_change_count()') + '\n'
    adapter += function(source, 'static bool handleSetShaderChanged(') + '\n'
    adapter += source[helper_start:helper_end] + '\n'
    adapter += function(source, 'static void handleRigRimWrapAmount(') + '\n'
    adapter += function(source, 'static void setupWeatherMaterialListeners()') + '\n'
    adapter += registration + '\n'
    def value(node):
        if node.tag == 'array':
            return 'LLSD::emptyArray()' + ''.join('.append(' + value(v) + ')' for v in node)
        if node.tag == 'map':
            children = list(node)
            return 'LLSD::emptyMap()' + ''.join('.with(' + json.dumps(children[i].text) + ',' + value(children[i+1]) + ')'
                                              for i in range(0, len(children), 2))
        if node.tag == 'string':
            return 'LLSD(' + json.dumps(node.text or '') + ')'
        if node.tag == 'boolean':
            return 'LLSD(' + ('true' if node.text in ('true', '1') else 'false') + ')'
        if node.tag in ('integer', 'real'):
            return 'LLSD(' + (node.text or '0') + ')'
        raise ValueError('Unsupported setting value: ' + node.tag)
    declarations = []
    types = {'U32': 'U32', 'S32': 'S32', 'F32': 'F32', 'Boolean': 'BOOLEAN', 'String': 'STRING',
             'Vector3': 'VEC3', 'Vector3D': 'VEC3D', 'Quaternion': 'QUAT', 'Rect': 'RECT',
             'Color4': 'COL4', 'Color3': 'COL3', 'LLSD': 'LLSD'}
    def declare_file(path, group, seen):
        if path in seen:
            return
        seen.add(path)
        nodes = list(ET.parse(path).getroot().find('map'))
        for i in range(0, len(nodes), 2):
            key, data = nodes[i].text, nodes[i+1]
            if key == 'Include':
                for included in data:
                    declare_file(path.parent / included.text, group, seen)
                continue
            fields = list(data)
            props = {fields[j].text: fields[j+1] for j in range(0, len(fields), 2)}
            declarations.append(f'{group}.declareControl({json.dumps(key)}, TYPE_{types[props["Type"].text]}, '
                                f'{value(props["Value"])}, "offline production default", LLControlVariable::PERSIST_NO);')
    settings_dir = ROOT / 'indra/newview/app_settings'
    declare_file(settings_dir / 'settings.xml', 'gSavedSettings', set())
    declare_file(settings_dir / 'settings_per_account.xml', 'gSavedPerAccountSettings', set())
    adapter += '\nvoid declare_test_settings() {\n' + '\n'.join(declarations) + '\n}\n'

    # Use the existing dependency build's exact ABI flags and library list.
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    project = ET.parse(dependency_build / 'llxml/llxml.vcxproj')
    groups = project.findall('m:ItemDefinitionGroup', ns)
    release = next(g for g in groups if "'Release|x64'" in g.attrib.get('Condition', ''))
    definitions = release.find('m:ClCompile/m:PreprocessorDefinitions', ns).text.split(';')
    flags = ['/nologo', '/std:c++20', '/EHsc', '/O2', '/MT', '/DAL_ENV_ONLY_SIGNAL_STANDALONE']
    flags += ['/D' + d for d in definitions if d and not d.startswith(('%', 'CMAKE_INTDIR', 'NDEBUG'))]
    flags += ['/utf-8', '/bigobj', '/Zc:__cplusplus']
    for directory in ('llcommon', 'llxml', 'llmath', 'llfilesystem', 'newview/tests'):
        flags.append('/I' + str(ROOT / 'indra' / directory))
    flags += ['/I' + str(out), '/I' + str(dependency_build / 'vcpkg_installed/x64-windows-alchemy/include')]
    flags += ['/I' + str(dependency_build / 'vcpkg_installed/x64-windows-alchemy/include/tracy')]
    link_project = dependency_build / 'integration_tests/llimage_libtest/llimage_libtest.vcxproj'
    link_tree = ET.parse(link_project)
    link_release = next(g for g in link_tree.findall('m:ItemDefinitionGroup', ns)
                        if "'Release|x64'" in g.attrib.get('Condition', ''))
    libraries = link_release.find('m:Link/m:AdditionalDependencies', ns).text.split(';')
    libraries = [str((link_project.parent / p).resolve()) if '\\' in p else p for p in libraries]
    libraries += [str(dependency_build / 'llxml/Release/llxml.lib')]
    # Compile llcontrol from this checkout, rather than trusting an old llxml object.
    inputs = [ROOT / 'indra/newview/tests/alenvintensity_envonly_test.cpp', ROOT / 'indra/llxml/llcontrol.cpp']
    def quote(arg):
        return '"' + str(arg) + '"'
    dev = Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat')
    cases = [('current', '', '')]
    for label, key, callback, assertion in (
        ('negative_mode', 'AlchemyEnvEnvironmentOnly', 'handleSetShaderChanged(value);', 'settings_shader_change_count() == handlers'),
        ('negative_tag', 'RenderGBufferAvatarTag', 'handleSetShaderChanged(value);', 'settings_shader_change_count() == handlers'),
        ('negative_direct', 'AlchemyEnvEnvironmentOnly', 'LLViewerShaderMgr::instance()->setShaders();', 'LLViewerShaderMgr::calls == shaders'),
    ):
        addition = f'''\n    gSavedSettings.getControl("{key}")->getSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&) {{ {callback} }});\n'''
        cases.append((label, addition, assertion))
    for label, addition, assertion in cases:
        (out / 'env_only_signal_adapter.h').write_text(
            adapter if not addition else adapter.replace(registration, registration[:-1] + addition + '}'), encoding='utf-8')
        exe = out / (label + '.exe')
        rsp = out / 'compile.rsp'
        rsp.write_text('\n'.join(map(quote, flags + list(map(str, inputs)) +
                       ['/c', '/Fo' + out.as_posix() + '/'])), encoding='utf-8')
        link_rsp = out / 'link.rsp'
        objects = [str(out / (path.stem + '.obj')) for path in inputs]
        link_rsp.write_text('\n'.join(map(quote, ['/LTCG', '/OUT:' + str(exe)] + objects + libraries)), encoding='utf-8')
        cmd = out / 'run.cmd'
        cmd.write_text(f'@echo off\ncall "{dev}" -arch=x64 -host_arch=x64 >nul\n'
                       f'cl @"{rsp}"\nif errorlevel 1 exit /b %errorlevel%\nlink @"{link_rsp}"\n', encoding='utf-8')
        try:
            result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=ROOT, capture_output=True, text=True, timeout=180)
        finally:
            # Negative registrations exist only in generated compile input.
            (out / 'env_only_signal_adapter.h').write_text(adapter, encoding='utf-8')
        (out / (label + '_compile.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        print(result.stdout, result.stderr, flush=True)
        if result.returncode:
            return False
        try:
            result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
        except subprocess.TimeoutExpired as error:
            print('FAIL signal test timed out:', error.stdout, error.stderr, flush=True)
            return False
        (out / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        print(label, 'exit', result.returncode, result.stdout, result.stderr, flush=True)
        if label == 'current' and result.returncode:
            return False
        if addition and (not result.returncode or assertion not in result.stderr):
            return False
    # Leave generated adapter clean too: negative registration never edits production.
    (out / 'env_only_signal_adapter.h').write_text(adapter, encoding='utf-8')
    print('PASS 3 negative controls: both reload listeners and a direct setShaders listener fail runtime assertions', flush=True)
    return True


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dependency-build', type=Path, default=ROOT.parent / 'alchemy-machinima/build-Windows-vs2026-os')
    raise SystemExit(not run(ap.parse_args().dependency_build))
