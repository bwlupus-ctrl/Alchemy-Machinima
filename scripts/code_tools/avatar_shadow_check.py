#!/usr/bin/env python3
"""Source contracts and standalone production C++ alpha/routing tests (no viewer build).

Extracts production methods unchanged and uses minimal GL/texture declarations.
MSVC flags and defines come from the actual Release llrender vcxproj. The in-tree
TUT test additionally covers texture uploads when BUILD_HEADLESS is available.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
BASE = '1f5a35e8086'


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def baseline(path):
    return subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=ROOT,
                                   text=True, encoding='utf-8', timeout=30)


def contracts():
    pipeline = read('indra/newview/pipeline.cpp')
    bias = function(pipeline, 'void LLPipeline::bindDeferredShader(LLGLSLShader& shader,')
    assert re.search(r'F32\s+shadow_bias\s*=\s*RenderShadowBias\s*;', bias), 'sun bias must be altitude independent'
    assert bias.count('F32 shadow_bias') == 1
    units = function(pipeline, 'void LLPipeline::uploadShadowUniforms(')
    assert 'shader.uniform1f(sShadowBiasMM,       llclamp(RenderShadowBiasMM, 0.2f, 50.f));' in units
    assert 'RenderShadowBiasError' not in re.sub(r'//[^\n]*', '', units)
    code = re.sub(r'//[^\n]*|/\*.*?\*/', '', bias, flags=re.S)
    assert not re.search(r'RenderShadowBiasError\s*\*|shadow_bias_error|3000\.f', code), 'altitude bias remains'
    geom = function(pipeline, 'void LLPipeline::renderGeomShadow(')
    assert 'i < poolp->getNumShadowPasses()' in geom
    assert 'poolp->beginShadowPass(i)' in geom and 'p->renderShadow(i)' in geom and 'poolp->endShadowPass(i)' in geom
    avatar = read('indra/newview/lldrawpoolavatar.cpp')
    begin = function(avatar, 'void LLDrawPoolAvatar::beginShadowPass(')
    assert begin.count('setMinimumAlpha(sMinimumAlpha)') == 2
    assert 'SHADOW_PASS_AVATAR_OPAQUE_CUT || pass == SHADOW_PASS_AVATAR_ALPHA_MASK' in begin
    assert 'sShadowBakes = SHADOW_BAKES_ALL;' in function(begin, 'else if (pass == SHADOW_PASS_AVATAR_ALPHA_BLEND)')
    assert 'sShadowBakes = pass == SHADOW_PASS_AVATAR_OPAQUE_CUT ? SHADOW_BAKES_CUT : SHADOW_BAKES_ALL;' in begin
    end = function(avatar, 'void LLDrawPoolAvatar::endShadowPass(')
    assert 'sShadowBakes = SHADOW_BAKES_ALL' in end
    shadow = function(avatar, 'void LLDrawPoolAvatar::renderShadow(')
    assert shadow.index('sShadowCutBakeLeft = false') < shadow.index('mShadowCutBakes = sShadowCutBakeLeft')
    assert 'else if (pass == SHADOW_PASS_AVATAR_OPAQUE_CUT)' in shadow
    assert shadow.index('pass == SHADOW_PASS_AVATAR_OPAQUE_CUT && !mShadowCutBakes') < shadow.index('get_actor_fx_id(')
    assert 'indices && !mShadowBodyCounted && LLPipeline::sWorldShadowDrawCounts' in shadow
    assert 'mShadowBodyCounted = false;' in shadow and 'mShadowBodyCounted = true;' in shadow
    for token in ('is_hidden_entity_clone', 'isGhostAvatar', 'isHardVisualMute', 'isImpostor',
                  'shouldSuppressSharedActorFx', 'uploadActorFx', 'sWorldShadowDrawCounts'):
        assert token in shadow, token
    mesh = function(read('indra/newview/llviewerjointmesh.cpp'), 'U32 LLViewerJointMesh::drawShape(')
    assert mesh.count('uploadJointMatrices()') == 1
    assert mesh.index('uploadJointMatrices()') < mesh.index('shadowDrawsBake(')
    assert 'shadowDrawsBake(nullptr, mShadowBakeFrame, mShadowBakeCut)' in mesh
    assert 'shadowDrawsBake(diffuse, mShadowBakeFrame, mShadowBakeCut)' in mesh
    joint = function(read('indra/newview/llviewerjoint.cpp'), 'U32 LLViewerJoint::render(')
    assert 'llassert(LLDrawPoolAvatar::sShadowBakes == LLDrawPoolAvatar::SHADOW_BAKES_ALL);' in joint
    image = read('indra/llrender/llimagegl.cpp')
    upload = function(image, 'bool LLImageGL::setImage(const U8*')
    assert 'U8 min_alpha = getDefaultMinAlpha();' in upload and '*min_alpha_out = min_alpha;' in upload
    assert 'syncTexName(new_tex_name, min_alpha)' in function(image, 'void LLImageGL::syncToMainThread(')
    sync = function(image, 'void LLImageGL::syncTexName(')
    assert sync.index('mTexName = texname;') < sync.index('mMinAlpha.store(min_alpha, std::memory_order_release)')
    assert 'std::atomic<U8> mMinAlpha' in read('indra/llrender/llimagegl.h')
    for signature in ('bool LLImageGL::setSubImage(const U8*',
                      'bool LLImageGL::setSubImageFromFrameBuffer(', 'void LLImageGL::setNeedsAlphaAndPickMask('):
        assert 'mMinAlpha.store(getDefaultMinAlpha(), std::memory_order_release)' in function(image, signature), signature
    assert 'mMinAlpha' not in function(image, 'U8 LLImageGL::analyzeAlpha(')
    assert 'mMinAlpha' not in function(image, 'void LLImageGL::calcAlphaChannelOffsetAndStride(')
    for path in (ROOT/'indra/newview/app_settings/shaders').rglob('*.glsl'):
        if any(name.lower().startswith(('reeelight', 'reshade', 'immerse', 'martysmods_relight')) for name in path.parts):
            continue
        text = path.read_text(encoding='utf-8')
        assert 'target_pos_x' not in text and 'shadow_target_width' not in text, path
    assert 'DEFERRED_SHADOW_TARGET_WIDTH' not in pipeline
    settings = ET.fromstring(read('indra/newview/app_settings/settings.xml'))
    settings_old = ET.fromstring(baseline('indra/newview/app_settings/settings.xml'))
    def value(tree, name):
        nodes = list(tree.find('map'))
        return nodes[next(i for i, n in enumerate(nodes) if n.tag == 'key' and n.text == name) + 1]
    assert ET.tostring(value(settings, 'RenderShadowBias')) == ET.tostring(value(settings_old, 'RenderShadowBias'))
    assert value(settings, 'RenderShadowBiasError').find('string').text == 'unused (kept for saved-settings compatibility)'
    detail_settings = ET.fromstring(read('indra/newview/app_settings/settings_alchemy.xml'))
    detail_nodes = list(value(detail_settings, 'RenderAvatarShadowDetail'))
    assert detail_nodes[next(i for i,n in enumerate(detail_nodes) if n.tag == 'key' and n.text == 'Value') + 1].text == '1'
    print('PASS source contracts: scoped sun/Units bias; upload before routing; atomic publication; pass state/counters; stable detail default; dead width removed', flush=True)


def cpp_source(mutation=None):
    image = read('indra/llrender/llimagegl.cpp')
    avatar = read('indra/newview/lldrawpoolavatar.cpp')
    header = read('indra/newview/lldrawpoolavatar.h')
    enums = '\n'.join(re.findall(r'(?:typedef )?enum(?: eShadowBakes)?\s*\{[^}]*SHADOW_[^}]*\}(?: eShadowPass)?;', header))
    assert 'NUM_SHADOW_PASSES' in enums and 'SHADOW_BAKES_CUT' in enums
    detail = avatar[avatar.index('    static LLCachedControl<S32> avatar_shadow_detail', avatar.index('void LLDrawPoolAvatar::renderShadow')):]
    detail = detail[:detail.index('    const LLUUID actor_fx_id')]
    mesh = function(read('indra/newview/llviewerjointmesh.cpp'), 'U32 LLViewerJointMesh::drawShape(')
    palette = function(mesh, '    if (first_pass && mMesh->hasWeights()')
    skip = function(mesh, '        if (!LLDrawPoolAvatar::shadowDrawsBake(diffuse,')
    # Preserve production order: the old routing-before-upload code must fail.
    ordered = '\n'.join(sorted((palette, skip), key=mesh.index, reverse=mutation == 'palette'))
    count = function(function(avatar, 'void LLDrawPoolAvatar::renderShadow('), '    if (indices && !mShadowBodyCounted')
    formats = ['LUMINANCE', 'RED', 'RG', 'RGB', 'BGR', 'SRGB', 'RED_INTEGER', 'RG_INTEGER',
               'RGB_INTEGER', 'RGBA_INTEGER', 'BGR_INTEGER', 'BGRA_INTEGER', 'RGBA', 'LUMINANCE_ALPHA', 'BGRA']
    source = r'''
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
using U8 = uint8_t; using U32 = uint32_t; using S8 = int8_t; using S32 = int32_t;
using F32 = float; using GLubyte = unsigned char;
using LLGLuint = unsigned int;
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
template<class T> T llmin(T a, T b) { return std::min(a,b); }
template<class T> T llmax(T a, T b) { return std::max(a,b); }
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
''' + '\n'.join('constexpr U32 GL_' + name + ' = ' + str(i + 1) + ';' for i, name in enumerate(formats)) + r'''
struct LLImageGL {
    U32 mDeprecatedSourceFormat = 0, mFormatPrimary = GL_RGBA;
    std::atomic<U8> mMinAlpha{0}; bool mIsMask = false, mNeedsAlphaAndPickMask = true;
    LLGLuint mTexName = 0;
    S8 mAlphaOffset = 3, mAlphaStride = 4;
    static inline bool sSkipAnalyzeAlpha = false;
    U8 getMinAlpha() const;
    U8 getDefaultMinAlpha() const;
    U8 analyzeAlpha(const void*, U32, U32);
    void syncTexName(LLGLuint, U8);
    static void deleteTextures(int, LLGLuint*) {}
    // Mirrors setImage's output value; analysis alone cannot alter visible metadata.
    void analysed(const void* data, U32 w, U32 h) {
        const U8 minimum = llmax(getDefaultMinAlpha(), analyzeAlpha(data,w,h));
        syncTexName(1,minimum);
    }
};
struct LLGLTexture {
    const LLImageGL* image;
    const LLImageGL* getGLTexture() const { return image; }
};
struct LLDrawPoolAvatar {
''' + enums + r'''
    static inline eShadowBakes sShadowBakes = SHADOW_BAKES_ALL;
    static inline bool sShadowCutBakeLeft = false;
    static inline F32 sMinimumAlpha = .2f;
    static bool shadowDrawsBake(const LLGLTexture*, U32&, bool&);
};
U32 gFrameCount = 1;
int gSavedSettings = 0, fixture_detail = 1;
template<class T> struct LLCachedControl {
    LLCachedControl(int, const char*, int) {}
    T operator()() const { return static_cast<T>(fixture_detail); }
};
void route(S32 pass, bool actor_fx_enabled, bool& draws) {
    using enum LLDrawPoolAvatar::eShadowPass;
''' + detail + r'''
    draws = true;
}
''' + '\n'.join((function(image, 'U8 LLImageGL::getMinAlpha() const'),
                 function(image, 'U8 LLImageGL::getDefaultMinAlpha() const'),
                 function(image, 'U8 LLImageGL::analyzeAlpha('),
                 function(image, 'void LLImageGL::syncTexName('),
                 function(avatar, 'bool LLDrawPoolAvatar::shadowDrawsBake('))) + r'''
U32 shared_palette = 0, uploads = 0;
struct FixtureMesh { bool weighted = true; bool hasWeights() const { return weighted; } };
struct FixturePool { S32 shader = 1; S32 getShaderLevel() const { return shader; } };
struct FixtureFace { FixturePool* pool; FixturePool* getPool() const { return pool; } };
struct OrderingMesh {
    FixtureMesh* mMesh; FixtureFace* mFace; U32 desired;
    U32 mShadowBakeFrame = ~0U; bool mShadowBakeCut = false;
    void uploadJointMatrices() { shared_palette=desired; ++uploads; }
    U32 draw(bool first_pass, const LLGLTexture* diffuse) {
''' + ordered + r'''
        return shared_palette;
    }
};
struct DrawCounts { U32 avatarBodies=0, selfBodies=0; };
struct LLPipeline { static inline DrawCounts* sWorldShadowDrawCounts=nullptr; };
struct FixtureAvatar { bool self; bool isSelf() const { return self; } };
struct CountingPool {
    bool mShadowBodyCounted=false;
    void record(U32 indices, const FixtureAvatar* avatarp) {
''' + count + r'''
    }
};
int main() {
    LLImageGL image;
    CHECK(image.getMinAlpha() == 0);
    const U32 no_alpha[] = {GL_LUMINANCE,GL_RED,GL_RG,GL_RGB,GL_BGR,GL_SRGB,GL_RED_INTEGER,
        GL_RG_INTEGER,GL_RGB_INTEGER,GL_RGBA_INTEGER,GL_BGR_INTEGER,GL_BGRA_INTEGER};
    for (U32 format : no_alpha) {
        image.mFormatPrimary=format; CHECK(image.getDefaultMinAlpha()==255);
        image.analysed(nullptr,0,0); CHECK(image.getMinAlpha()==255);
    }
    image.mFormatPrimary=GL_RG; image.mDeprecatedSourceFormat=GL_LUMINANCE_ALPHA;
    CHECK(image.getDefaultMinAlpha()==0); image.mDeprecatedSourceFormat=0; image.mFormatPrimary=GL_RGBA;
    // Every position, including odd edges, strides and offsets; real minimum must survive.
    for (U32 w : {1u,2u,9u,15u,16u}) for (U32 h : {1u,2u,9u,15u,16u}) {
        for (S8 stride : {S8{1},S8{2},S8{4}}) for (S8 offset : {S8{0},static_cast<S8>(stride-1)}) {
            image.mAlphaStride=stride; image.mAlphaOffset=offset;
            std::vector<U8> pixels(static_cast<size_t>(w)*h*stride,255);
            image.analysed(pixels.data(),w,h); CHECK(image.getMinAlpha()==255);
            for (size_t p=0;p<static_cast<size_t>(w)*h;++p) {
                pixels[p*stride+offset]=37; image.analysed(pixels.data(),w,h);
                CHECK(image.getMinAlpha()==37); pixels[p*stride+offset]=255;
            }
            image.analysed(nullptr,w,h); CHECK(image.getMinAlpha()==0);
            image.mNeedsAlphaAndPickMask=false; image.analysed(pixels.data(),w,h);
            CHECK(image.getMinAlpha()==0); image.mNeedsAlphaAndPickMask=true;
            image.sSkipAnalyzeAlpha=true; image.analysed(pixels.data(),w,h);
            CHECK(image.getMinAlpha()==0); image.sSkipAnalyzeAlpha=false;
        }
    }
    image.analysed(nullptr,0,0); CHECK(image.getMinAlpha()==0);
    // An upload analysis does not publish; the captured result travels with its GL name.
    image.mAlphaStride=4; image.mAlphaOffset=3;
    std::vector<U8> opaque_pixels(16,255), cut_pixels(16,0);
    image.mTexName=11;
    const U8 opaque_min=image.analyzeAlpha(opaque_pixels.data(),2,2);
    CHECK(opaque_min==255); CHECK(image.getMinAlpha()==0); CHECK(image.mTexName==11);
    const U8 cut_min=image.analyzeAlpha(cut_pixels.data(),2,2);
    CHECK(cut_min==0); CHECK(image.getMinAlpha()==0);
    image.syncTexName(12,opaque_min); CHECK(image.mTexName==12); CHECK(image.getMinAlpha()==255);
    image.syncTexName(13,cut_min); CHECK(image.mTexName==13); CHECK(image.getMinAlpha()==0);
    using A = LLDrawPoolAvatar;
    CHECK(A::NUM_SHADOW_PASSES==4);
    LLGLTexture bake{&image}, unknown{nullptr};
    U32 frame=~0U; bool cut=false;
    for (U32 alpha : {0u,1u,50u,51u,52u,254u,255u}) {
        image.mMinAlpha=static_cast<U8>(alpha);
        ++gFrameCount;
        A::sShadowBakes=A::SHADOW_BAKES_SOLID; A::sShadowCutBakeLeft=false;
        const bool solid=A::shadowDrawsBake(&bake,frame,cut);
        CHECK(solid==(alpha>=51)); CHECK(A::sShadowCutBakeLeft==!solid);
        A::sShadowBakes=A::SHADOW_BAKES_CUT;
        CHECK(A::shadowDrawsBake(&bake,frame,cut)==!solid);
        A::sShadowBakes=A::SHADOW_BAKES_ALL; CHECK(A::shadowDrawsBake(&bake,frame,cut));
    }
    A::sShadowBakes=A::SHADOW_BAKES_SOLID; A::sShadowCutBakeLeft=false;
    ++gFrameCount;
    CHECK(!A::shadowDrawsBake(nullptr,frame,cut)); CHECK(!A::shadowDrawsBake(&unknown,frame,cut));
    CHECK(A::sShadowCutBakeLeft);
    image.mMinAlpha=255; ++gFrameCount;
    CHECK(A::shadowDrawsBake(&bake,frame,cut)); CHECK(A::sShadowCutBakeLeft);
    A::sShadowBakes=A::SHADOW_BAKES_CUT; ++gFrameCount;
    CHECK(A::shadowDrawsBake(nullptr,frame,cut)); CHECK(A::shadowDrawsBake(&unknown,frame,cut));
    // Deterministic mid-pass upload completion, in both directions, across two maps.
    for (U8 before : {U8{0},U8{255}}) {
        ++gFrameCount; image.syncTexName(20,before);
        A::sShadowBakes=A::SHADOW_BAKES_SOLID;
        const bool solid=A::shadowDrawsBake(&bake,frame,cut);
        const U8 after=before==0?U8{255}:U8{0}; image.syncTexName(21,after);
        A::sShadowBakes=A::SHADOW_BAKES_CUT;
        CHECK(A::shadowDrawsBake(&bake,frame,cut)!=solid);
        A::sShadowBakes=A::SHADOW_BAKES_SOLID; CHECK(A::shadowDrawsBake(&bake,frame,cut)==solid);
        ++gFrameCount;
        CHECK(A::shadowDrawsBake(&bake,frame,cut)==(after==255));
    }
    FixtureMesh weighted; FixturePool pool; FixtureFace face{&pool};
    LLImageGL head_image, body_image; LLGLTexture head{&head_image}, body{&body_image};
    // A skipped first head still supplies the shared palette to upper/lower.
    for (U32 desired : {57u,93u}) for (bool cut_pass : {false,true}) {
        ++gFrameCount; shared_palette=999; uploads=0;
        head_image.mMinAlpha=cut_pass?255:0; body_image.mMinAlpha=cut_pass?0:255;
        OrderingMesh first{&weighted,&face,desired}, later{&weighted,&face,desired};
        A::sShadowBakes=cut_pass?A::SHADOW_BAKES_CUT:A::SHADOW_BAKES_SOLID;
        CHECK(first.draw(true,&head)==0); CHECK(uploads==1); CHECK(shared_palette==desired);
        CHECK(later.draw(false,&body)==desired); CHECK(uploads==1);
    }
    for (bool self : {false,true}) for (bool cut_only : {false,true}) {
        DrawCounts counts; LLPipeline::sWorldShadowDrawCounts=&counts;
        CountingPool pool_count; FixtureAvatar actor{self};
        for (U32 map=1;map<=2;++map) {
            pool_count.mShadowBodyCounted=false;
            pool_count.record(cut_only?0u:6u,&actor); // opaque
            pool_count.record(6,&actor); // cut
            pool_count.record(6,&actor); // alpha
            CHECK(counts.avatarBodies==map); CHECK(counts.selfBodies==(self?map:0));
        }
    }
    for (int detail : {-1,0,1,2,3}) for (bool actor : {false,true}) {
        fixture_detail=detail;
        for (S32 pass=0;pass<A::NUM_SHADOW_PASSES;++pass) {
            bool draws=false; route(pass,actor,draws);
            const int effective=actor?std::max(detail,1):detail;
            const bool expected=pass==A::SHADOW_PASS_AVATAR_OPAQUE || pass==A::SHADOW_PASS_AVATAR_OPAQUE_CUT ||
                (pass==A::SHADOW_PASS_AVATAR_ALPHA_MASK?effective==1:effective>=2);
            CHECK(draws==expected);
        }
    }
    std::cout << "PASS production C++: alpha minima; atomic name/alpha publication; frozen routing despite mid-pass flips; skipped-first palette ordering; detail/Actor FX routing\n";
}
'''
    if mutation == 'snapshot':
        assert source.count('if (frame != gFrameCount)') == 1
        source = source.replace('if (frame != gFrameCount)', 'if (true)')
    return source


def compile_and_run(out, project, mutation=None):
    out.mkdir(parents=True, exist_ok=True)
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    tree = ET.parse(project)
    config = next(n for n in tree.findall('m:ItemDefinitionGroup', ns) if "'Release|x64'" in n.get('Condition', ''))
    cl = config.find('m:ClCompile', ns)
    def setting(name):
        return cl.find('m:' + name, ns).text
    assert setting('FloatingPointModel') == 'Fast' and setting('TreatWarningAsError') == 'true'
    assert setting('WarningLevel') == 'Level3' and setting('LanguageStandard') == 'stdcpp20'
    assert setting('RuntimeLibrary') == 'MultiThreaded' and setting('Optimization') == 'MaxSpeed'
    flags = ['/nologo', '/O2', '/Ob3', '/utf-8', '/bigobj', '/Zc:__cplusplus', '/arch:AVX2', '/EHsc',
             '/external:W0', '/fp:fast', '/Gy', '/std:c++20', '/MT', '/GR', '/GS', '/permissive-',
             '/Zc:wchar_t', '/Zc:preprocessor', '/W3', '/WX', '/Zi']
    flags += ['/D' + d for d in setting('PreprocessorDefinitions').split(';') if d and not d.startswith('%(')]
    source = out / 'production_cpu.cpp'
    source.write_text(cpp_source(mutation), encoding='utf-8')
    exe = out / 'production_cpu.exe'
    flags += ['/Fo' + str(out / 'production_cpu.obj'), '/Fd' + str(out / 'production_cpu.pdb'),
              str(source), '/Fe' + str(exe), '/link', '/INCREMENTAL:NO']
    vcvars = Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat')
    batch = out / 'compile_cpu.cmd'
    command = subprocess.list2cmdline(['cl.exe', *flags])
    batch.write_text('@echo off\ncall "' + str(vcvars) + '" >nul\nif errorlevel 1 exit /b %errorlevel%\n' + command + '\n', encoding='utf-8')
    (out / 'release_flags.json').write_text(json.dumps(dict(project=str(project), flags=flags), indent=2), encoding='utf-8')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=out, capture_output=True, text=True, timeout=120)
    (out / 'compile_cpu.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    print(result.stdout + result.stderr, end='', flush=True)
    assert result.returncode == 0, 'MSVC production-method test compile failed'
    result = subprocess.run([str(exe)], cwd=out, capture_output=True, text=True, timeout=30)
    (out / 'run_cpu.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    if mutation:
        expected = 'uploads==1' if mutation == 'palette' else 'A::shadowDrawsBake(&bake,frame,cut)!=solid'
        assert result.returncode != 0 and expected in result.stderr, 'CPU negative control failed at the wrong assertion: ' + result.stdout + result.stderr
        print('PASS rejected CPU negative control:', mutation, result.stderr.strip(), flush=True)
    else:
        print(result.stdout + result.stderr, end='', flush=True)
        assert result.returncode == 0, 'production CPU test failed'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / '.tmp/avatar_shadow_20261028/cpu')
    parser.add_argument('--project', type=Path, default=Path('I:/alchemy-machinima/build-Windows-vs2026-os/llrender/llrender.vcxproj'))
    args = parser.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    contracts()
    compile_and_run(args.out, args.project)
    for mutation in ('palette', 'snapshot'):
        compile_and_run(args.out / ('negative_' + mutation), args.project, mutation)
