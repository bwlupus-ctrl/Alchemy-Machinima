#!/usr/bin/env python3
"""Run real TUT/parser/production-path tests and check diagnostic settings metadata.

Requires MSVC x64, configured CMake/vcpkg dependencies and existing Release
llcommon/llmath libraries. Missing prerequisites are a hard failure, including
when this script is run as an unconditional prebuild gate.
"""
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags
from shader_load_check import function


def check_log_messages():
    # Immutable local pre-Step-1 baseline, also valid after this branch is committed.
    base = '456d04d387f3cb7bb1494ea276b111e9337dd550'
    def messages(body):
        expressions = re.findall(r'\bLL_(?:DEBUGS|INFOS|WARNS|ERRS|SHADER_LOADING_WARNS)\([^;]*?LL_ENDL;', body, re.S)
        # Preserve every byte inside literals, including whitespace and escapes.
        return sorted(''.join(re.findall(r'"(?:\\.|[^"\\])*"|[^\s]', e)) for e in expressions)
    count = 0
    for path, cls, names in (
            ('indra/llrender/llglslshader.cpp', 'LLGLSLShader',
             ('createShader', 'attachVertexObject', 'attachFragmentObject', 'attachObject', 'mapAttributes', 'link')),
            ('indra/llrender/llshadermgr.cpp', 'LLShaderMgr',
             ('attachShaderFeatures', 'loadShaderFile', 'linkProgramObject', 'loadCachedProgramBinary'))):
        before = subprocess.check_output(['git', 'show', base + ':' + path], cwd=ROOT, text=True, encoding='utf-8')
        after = (ROOT / path).read_text(encoding='utf-8')
        for name in names:
            expected = messages(function(before, cls + '::' + name + '('))
            body = function(after, cls + '::' + name + 'Internal(')
            split = {
                'createShader': ('prepareShaderInternal', 'submitShaderInternal', 'finishShaderInternal'),
                'mapAttributes': ('bindAttributes', 'readAttributes'),
                'link': ('submitLink', 'finishLinkInternal'),
                'linkProgramObject': ('submitProgramLink', 'finishProgramLinkInternal'),
            }
            for helper in split.get(name, ()):
                body += function(after, cls + '::' + helper + '(')
            if name == 'loadCachedProgramBinary':
                body += function(after, cls + '::rejectProgramBinary(')
            assert messages(body) == expected, f'changed log message expression: {cls}::{name}'
            count += len(expected)
    assert messages('LL_INFOS() << "a b" << LL_ENDL;') != messages('LL_INFOS() << "ab" << LL_ENDL;')
    # All function-filter arrays in the checked-in logging configuration are empty.
    config = ET.parse(ROOT / 'indra/newview/app_settings/logcontrol.xml')
    for mapping in config.iter('map'):
        nodes = list(mapping)
        for key, value in zip(nodes[::2], nodes[1::2]):
            if key.text == 'functions':
                assert not list(value), 'recheck moved log identities against configured function filters'
    viewer = (ROOT / 'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    constructor = function(viewer, 'LLViewerShaderMgr::LLViewerShaderMgr(')
    setter = function(viewer, 'LLViewerShaderMgr::setShaders(')
    assert 'configureShaderDiagnostics(' in constructor
    assert setter.index('configureShaderDiagnostics(') < setter.index('if (!gPipeline.mInitialized')
    assert 'gSavedSettings' not in (ROOT / 'indra/llrender/llshadermgr.cpp').read_text(encoding='utf-8')
    print(f'log/startup gate: {count} existing log message expressions byte-equivalent; empty configured function filters; constructor and pre-guard configuration PASS')


def native_failure_paths(directory):
    shader = (ROOT / 'indra/llrender/llglslshader.cpp').read_text(encoding='utf-8')
    manager = (ROOT / 'indra/llrender/llshadermgr.cpp').read_text(encoding='utf-8')
    vertex = function(shader, 'LLGLSLShader::attachVertexObjectInternal(')
    proposed = vertex.replace('attachVertexObjectInternal(', 'attachVertexObjectProposed(', 1)
    proposed = proposed.replace('{\n', '{\n    if (mUsingBinaryProgram) return true;\n', 1)
    link = '\n'.join(function(manager, 'LLShaderMgr::' + n + '(') for n in ('submitProgramLink', 'finishProgramLinkInternal', 'linkProgramObjectInternal'))
    matcher = function(manager, 'LLShaderMgr::injectShaderFailure(')
    source = r'''
#include "llshaderdiagnostics.h"
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
using GLuint=unsigned;using GLenum=unsigned;using GLint=int;using S32=int;
constexpr GLint GL_TRUE=1,GL_FALSE=0;constexpr GLenum GL_VERTEX_SHADER=35633,GL_LINK_STATUS=35714;
int attachments=0,links=0,logs=0;bool driver_linked=true;
void stop_glerror(){}
void glAttachShader(GLuint,GLuint){++attachments;}
void glLinkProgram(GLuint){++links;}
void glGetProgramiv(GLuint,GLenum,GLint* status){*status=driver_linked;}
#define LL_PROFILE_ZONE_NAMED_CATEGORY_SHADER(x)
#define LL_SHADER_LOADING_WARNS() std::ostringstream()
#define LL_ENDL std::endl
struct LLShaderMgr {
    static LLShaderMgr* instance();
    std::map<std::string,GLuint> mVertexShaderObjects;
    GLuint shaderFeatureObject(const std::string& file,GLenum) const {
        auto it=mVertexShaderObjects.find(file);
        return it==mVertexShaderObjects.end()?0:it->second;
    }
    std::vector<LLShaderInjection> mShaderInjections;
    void traceFeatureAttachment(const std::string&,GLenum){}
    void traceAttachment(GLuint){}
    void dumpObjectLog(GLuint,bool,const std::string&){++logs;}
    bool injectShaderFailure(const std::string&,const std::string&,S32=-1) const;
    void submitProgramLink(GLuint);
    template<bool diagnostics> bool finishProgramLinkInternal(GLuint,bool,const std::string&);
    template<bool diagnostics> bool linkProgramObjectInternal(GLuint,bool,const std::string&);
} manager;
LLShaderMgr* LLShaderMgr::instance(){return &manager;}
struct LLGLSLShader {
    bool mUsingBinaryProgram=true;GLuint mProgramObject=1;
    template<bool diagnostics> bool attachVertexObjectInternal(std::string);
    template<bool diagnostics> bool attachVertexObjectProposed(std::string);
};
'''
    source += '\n' + vertex + '\n' + proposed + '\n' + link + '\n' + matcher
    source += r'''
int main(){
    LLGLSLShader shader;
    assert(!shader.attachVertexObjectInternal<false>("missing.glsl"));
    assert(shader.attachVertexObjectProposed<false>("missing.glsl"));
    manager.mVertexShaderObjects["present.glsl"]=2;
    assert(shader.attachVertexObjectInternal<false>("present.glsl") && attachments==1);
    assert(shader.attachVertexObjectProposed<false>("present.glsl") && attachments==1);
    std::vector<std::string> errors;
    manager.mShaderInjections=LLShaderInjection::parse("link:p;compile:a:3;missing:lib.glsl",errors);
    assert(manager.injectShaderFailure("compile","a",3));
    assert(!manager.injectShaderFailure("compile","a",2));
    assert(manager.injectShaderFailure("missing","lib.glsl"));
    assert(!manager.linkProgramObjectInternal<true>(1,false,"p") && logs==1);
    assert(manager.linkProgramObjectInternal<true>(1,false,"other"));
    assert(manager.linkProgramObjectInternal<false>(1,false,"p"));
    driver_linked=false;
    assert(!manager.linkProgramObjectInternal<false>(1,false,"other") && logs==2);
    assert(links==4);
    std::cout<<"native production paths: injected link uses real failure branch; OFF ignores injection; cached vertex guard changes return/attachments PASS\n";
}
'''
    result = compile_run(directory, source, name='failure_paths', flags=[f'/I"{ROOT / "indra/llrender"}"', '/O2', '/WX'])
    print(result.stdout, end='')
    assert result.returncode == 0, result.stderr


def main():
    # Unconditional prebuild gate: fail clearly before any partial native checks.
    viewer_native_flags()
    elements = list(ET.parse(ROOT / 'indra/newview/app_settings/settings.xml').getroot().find('map'))
    for name, kind, default in (
            ('RenderShaderDumpEquivalence', 'Boolean', '0'),
            ('RenderShaderInjectFail', 'String', '')):
        matches = [value for key, value in zip(elements[::2], elements[1::2]) if key.text == name]
        assert len(matches) == 1, name
        items = list(matches[0])
        fields = {key.text: value.text or '' for key, value in zip(items[::2], items[1::2])}
        assert fields['Type'] == kind and fields['Value'] == default and fields['Persist'] == '0', name
    source = (ROOT / 'indra/llrender/tests/llshaderdiagnostics_test.cpp').read_text(encoding='utf-8')
    source = source.replace('#include "linden_common.h"', '')
    source = source.replace('#include "../test/lltut.h"', '#include <tut/tut.hpp>\n#include <tut/tut_reporter.hpp>')
    source = source.replace('#include "../llshaderdiagnostics.h"', '#include "llshaderdiagnostics.h"')
    source += '''
namespace tut { test_runner_singleton runner; }
int main() {
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();
    return reporter.all_ok() ? 0 : 1;
}
'''
    directory = check_directory('shader_diagnostics')
    # TUT's dependency on boost is header-only. Reuse the existing build's
    # installed include directory; no dependency installation or viewer build.
    common = Path(subprocess.check_output(['git', 'rev-parse', '--git-common-dir'], cwd=ROOT, text=True).strip()).resolve().parent
    includes = sorted(common.glob('build-*/vcpkg_installed/*/include'))
    flags = ['/O2', '/WX', f'/I"{ROOT / "indra/llrender"}"', f'/I"{ROOT / "indra/externals/tut"}"']
    flags.extend(f'/I"{path}"' for path in includes)
    result = compile_run(directory, source, flags=flags)
    print(result.stdout, end='')
    assert result.returncode == 0, result.stderr
    native_failure_paths(directory)
    from shader_parallel_fixture import run as parallel_checks
    parallel_checks(directory)
    from shader_diagnostics_fixture import run
    run(directory)
    check_log_messages()
    print('shader_diagnostics_check: PASS (native TUT and diagnostic setting defaults/types/Persist)')


if __name__ == '__main__':
    main()
