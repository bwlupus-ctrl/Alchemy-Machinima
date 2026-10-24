"""Shadow Sculpt preset gate; Windows, Python numpy/Pillow, MSVC developer shell.

PYTHONUTF8=1 python scripts/code_tools/rig_shadow_presets_check.py
No viewer build. Uses the production loader/model, WGL lighting/shadow shaders,
and the existing .06 p95/.05 pop motion limits. Missing measurements FAIL.
Artifacts (including FAIL results) go to .tmp/rig_shadow. Uses the shipped cold-default bias master. Saved world-pick
intent controls ON; native-only OFF remains a separate measured path.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
XML = 'indra/newview/app_settings/cine_light_rig_presets.xml'
BASE = '13028c968b4'
PREFIX = 'Shadow Sculpt - '
FAMILIES = dict(zip([
    'Crisp Brow Cut', 'Crisp Fabric Rake', 'Crisp Cheek Chisel',
    'Crisp Low Three Quarter', 'Soft Deep Window', 'Soft Overhead Carve',
    'Soft Grazing Skin', 'Soft Low Contour', 'Edge Twin Kickers',
    'Edge Crown Blade', 'Edge Single Sliver', 'Edge Low Separation',
], ['crisp']*4 + ['soft']*4 + ['silhouette']*4))


def llsd(node):
    if node.tag == 'llsd':
        return llsd(node[0])
    if node.tag == 'map':
        items = list(node)
        assert len(items) % 2 == 0
        keys = [n.text for n in items[::2]]
        assert len(set(keys)) == len(keys), 'duplicate LLSD key'
        return {k: llsd(v) for k, v in zip(keys, items[1::2])}
    if node.tag == 'array':
        return [llsd(n) for n in node]
    if node.tag == 'boolean':
        return node.text in ('true', '1')
    if node.tag in ('real', 'integer'):
        return (float if node.tag == 'real' else int)(node.text)
    assert node.tag == 'string', node.tag
    return node.text or ''


def settings_defaults():
    # settings.xml already contains duplicate legacy top-level entries. Follow
    # LLSD's final-write behavior there; preset files remain strictly unique.
    items=list(ET.parse(ROOT / 'indra/newview/app_settings/settings.xml').getroot().find('map'))
    result={}
    for i,k in enumerate(items[:-1]):
        if k.tag!='key' or items[i+1].tag!='map':continue
        values=list(items[i+1])
        for j,v in enumerate(values[:-1]):
            if v.tag=='key' and v.text=='Value':result[k.text]=llsd(values[j+1])
    return result


def function(source, signature):
    start = source.index(signature)
    brace = source.index('\n{', start) + 1
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


SHIM = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "alcinelightrigmodel.h"
#include "alareashadowmath.h"
#include "alworldshadowmath.h"
using namespace ALCineLightRigModel;
struct LLSD {
    enum Type { NIL, MAP, ARRAY, STRING, REAL, INTEGER, BOOL } type=NIL;
    double number=0; std::string text;
    std::map<std::string,LLSD> fields; std::vector<LLSD> values;
    LLSD()=default;
    LLSD(double v):type(REAL),number(v){}
    LLSD(int v):type(INTEGER),number(v){}
    LLSD(bool v):type(BOOL),number(v){}
    LLSD(const char* v):type(STRING),text(v){}
    bool isMap() const{return type==MAP;}
    bool isArray() const{return type==ARRAY;}
    bool isString() const{return type==STRING;}
    bool isReal() const{return type==REAL;}
    bool isInteger() const{return type==INTEGER;}
    bool has(const char* k)const{return fields.count(k)!=0;}
    int size()const{return int(values.size());}
    double asReal()const{return number;}
    int asInteger()const{return int(number);}
    bool asBoolean()const{return number!=0;}
    void append(const LLSD& value){type=ARRAY;values.push_back(value);}
    std::string asString()const{return text;}
    LLSD& operator[](const char* k){type=MAP;return fields[k];}
    const LLSD& operator[](const char* k)const{static LLSD n;auto i=fields.find(k);return i==fields.end()?n:i->second;}
    const LLSD& operator[](int k)const{return values.at(k);}
    static LLSD emptyMap(){LLSD v;v.type=MAP;return v;}
    static LLSD emptyArray(){LLSD v;v.type=ARRAY;return v;}
};
'''


def cpp_data(value, path, lines):
    if isinstance(value, dict):
        lines.append(f'{path}=LLSD::emptyMap();')
        for k, v in value.items():
            cpp_data(v, path+'['+json.dumps(k)+']', lines)
    elif isinstance(value, list):
        lines.append(f'{path}=LLSD::emptyArray();')
        for v in value:
            # operator[](int) intentionally const; populate arrays via values.
            lines.append(f'{path}.values.emplace_back();')
            cpp_data(v, path+'.values.back()', lines)
    else:
        expr = json.dumps(value, ensure_ascii=True)
        lines.append(f'{path}=LLSD({expr});')


def baseline_model_outputs(out, presets, baseline):
    """Compile BASE and WORKTREE loaders/models separately; compare all outputs.

    Explicit members avoid struct padding and preserve every float's exact value.
    Neither binary can include the other revision's model header or implementation.
    """
    from relight_3d_gl import SourceTree
    outputs=[]
    for ref, rows in ((BASE,baseline),('WORKTREE',presets[:len(baseline)])):
        tree=SourceTree(ref);d=out/'regression'/ref;d.mkdir(parents=True,exist_ok=True)
        for name in ('alcinelightrigmodel.h','alcinelightrigmodel.cpp'):
            (d/name).write_text(tree.read('indra/newview/'+name),encoding='utf-8')
        (d/'stdtypes.h').write_text('#pragma once\n#include <cstdint>\nusing F32=float;using F64=double;using S32=int;using U32=uint32_t;using U64=uint64_t;using S64=int64_t;using U8=uint8_t;\n')
        (d/'linden_common.h').write_text('#include "stdtypes.h"\n')
        src=tree.read('indra/newview/alcinelightrig.cpp')
        signatures=[f'S32 {n}Index(const std::string& name)' for n in ('profile','beam','gobo','gel','fixtureGel','fixturePreset','flickerProgram')]
        loader='\n'.join(function(src,s) for s in signatures)
        loader+='\n'+function(src,'bool setupFromLLSD(const LLSD& data, Setup& output)')
        lines=[SHIM,loader,'void dump(const RigFrame& f){std::cout<<std::hexfloat;']
        members=('mOffX','mOffY','mOffZ','mAimX','mAimY','mAimZ','mSR','mSG','mSB','mIntensity','mLightRadius','mFalloff','mFovRad','mOn','mClipped','mGobo')
        lines.append('for(int k=0;k<8;++k){const auto& e=k<4?f.mProj[k]:f.mOmni[k-4];')
        lines.extend(f'std::cout<<e.{m}<<",";' for m in members)
        lines.append('}for(auto v:f.mDerivedShadowSoftness)std::cout<<v<<",";std::cout<<f.mCatchlightSizeScale<<"\\n";}')
        for i,row in enumerate(rows):
            lines.append(f'void regression_{i}(){{LLSD a;')
            cpp_data(row,'a',lines)
            lines.append('Setup s;assert(setupFromLLSD(a,s));LightBase live[4];computeLive(s,{},live);RigFrame f;render(s.mRadius,live,{},f);dump(f);}')
        lines.append('int main(){'+''.join(f'regression_{i}();' for i in range(len(rows)))+'}')
        lines.append('#include "alcinelightrigmodel.cpp"')
        (d/'regression.cpp').write_text('\n'.join(lines),encoding='utf-8')
        vc=r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
        cmd=d/'build.cmd'
        cmd.write_text(f'@echo off\ncall "{vc}" >nul\ncl /nologo /std:c++20 /utf-8 /EHsc /I"{d}" /I"{ROOT/"indra/newview"}" "{d/"regression.cpp"}" /Fe:"{d/"regression.exe"}" /Fo:"{d/"regression.obj"}"\nexit /b %ERRORLEVEL%\n')
        build=subprocess.run(['cmd','/d','/c',str(cmd)],capture_output=True,text=True)
        (d/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
        assert build.returncode==0,build.stdout+build.stderr
        run=subprocess.run([str(d/'regression.exe')],capture_output=True,text=True)
        assert run.returncode==0,run.stderr
        (d/'outputs.txt').write_text(run.stdout,encoding='utf-8');outputs.append(run.stdout.splitlines())
    assert len(outputs[0])==113 and outputs[0]==outputs[1], 'BASE model outputs changed for existing presets'
    print('[shadow-presets] 113 BASE-revision loader/model outputs byte-identical',flush=True)


def compile_model(out, presets, baseline, defaults):
    """Only storage/LLSD parsing is doubled; execute loader, math and ranking."""
    d = out / 'model'
    d.mkdir(parents=True, exist_ok=True)
    types = '#pragma once\n#include <cstdint>\nusing F32=float;using F64=double;using S32=int;using U32=uint32_t;using U64=uint64_t;using S64=int64_t;using U8=uint8_t;\n'
    (d / 'stdtypes.h').write_text(types)
    (d / 'linden_common.h').write_text('#include "stdtypes.h"\n')
    src = (ROOT / 'indra/newview/alcinelightrig.cpp').read_text(encoding='utf-8')
    signatures = [f'S32 {n}Index(const std::string& name)' for n in
                  ('profile', 'beam', 'gobo', 'gel', 'fixtureGel', 'fixturePreset', 'flickerProgram')]
    loader = '\n'.join(function(src, s) for s in signatures)
    loader += '\n'+function(src, 'bool setupFromLLSD(const LLSD& data, Setup& output)')
    loader += '\n'+function(src, 'LLSD lightToLLSD(const LightBase& light)')
    lines = [SHIM, loader]
    baseline_model_outputs(out,presets,baseline)
    lines.append('int main(){std::cout<<std::setprecision(9);')
    lines += ['assert(!ALWorldShadow::worldEligibleWithNative(true,false));',
              'assert(ALWorldShadow::worldEligibleWithNative(true,true));',
              'assert(!ALWorldShadow::channelUsesNativeMap(true,true,true));']
    lines.append('std::cout<<"[";')
    for j, p in enumerate(presets[len(baseline):]):
        lines.append('{LLSD a;')
        cpp_data(p, 'a', lines)
        lines.append('Setup s;assert(setupFromLLSD(a,s));LightBase live[4];computeLive(s,{},live);Globals g;')
        lines.append('LLSD saved=a;for(int i=0;i<4;++i)saved["lights"].values[i]=lightToLLSD(s.mLights[i]);Setup loaded;assert(setupFromLLSD(saved,loaded));for(int i=0;i<4;++i){assert(loaded.mLights[i].mDetailShadows==s.mLights[i].mDetailShadows);assert(loaded.mLights[i].mWorldShadowPick==s.mLights[i].mWorldShadowPick);auto mixed=blendLight(LightBase{},s.mLights[i],1.f);assert(mixed.mDetailShadows==s.mLights[i].mDetailShadows&&mixed.mWorldShadowPick==s.mLights[i].mWorldShadowPick);}')
        for member, value in dict(mHeadroomStops=defaults['CineLightRigHeadroomStops'],mBounceRatio=defaults['CineLightRigBounceRatio'],mMasterEV=p['master_ev']).items():
            lines.append(f'g.{member}={float(value):.9f}f;')
        lines.append('RigFrame f;render(s.mRadius,live,g,f);')
        lines.append('std::cout<<'+json.dumps(',' if j else '')+'<<"[";')
        lines.append(r'''for(int k=0;k<8;++k){int i=k%4;auto e=k<4?f.mProj[i]:f.mOmni[i];
        auto linear=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
        double lum=(.2126*linear(e.mSR)+.7152*linear(e.mSG)+.0722*linear(e.mSB))*e.mIntensity;
        double distance=std::sqrt(std::pow(e.mOffX-3.5,2)+e.mOffY*e.mOffY+std::pow(e.mOffZ+1.45-1.65,2));
        double score=ALAreaShadow::importance(lum,e.mLightRadius*1.5,distance,k==0?1.:0.);
        std::cout<<(k?",":"")<<"{\"on\":"<<e.mOn<<",\"pos\":["<<e.mOffX<<","<<e.mOffY<<","<<e.mOffZ<<"],\"rgb\":["<<e.mSR<<","<<e.mSG<<","<<e.mSB<<"],\"intensity\":"<<e.mIntensity<<",\"range\":"<<e.mLightRadius*1.5f<<",\"falloff\":"<<e.mFalloff<<",\"fov\":"<<e.mFovRad<<",\"softness\":"<<f.mDerivedShadowSoftness[i]<<",\"score\":"<<score<<",\"detail_shadows\":"<<(k<4&&live[i].mDetailShadows)<<",\"world_shadow_pick\":"<<(k<4&&live[i].mWorldShadowPick)<<",\"radius\":"<<live[i].mSourceSizeM*.5f<<"}";
        }std::cout<<"]";}''')
    lines.append('std::cout<<"]";}')
    lines.append('#include "'+str(ROOT/'indra/newview/alcinelightrigmodel.cpp').replace('\\','/')+'"')
    (d / 'model.cpp').write_text('\n'.join(lines), encoding='utf-8')
    vc = r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
    (d / 'build.cmd').write_text('@echo off\ncall "'+vc+'" >nul\ncl /nologo /std:c++20 /utf-8 /EHsc /I"'+str(d)+'" /I"'+str(ROOT/'indra/newview')+'" "'+str(d/'model.cpp')+'" /Fe:"'+str(d/'model.exe')+'" /Fo:"'+str(d/'model.obj')+'"\nexit /b %ERRORLEVEL%\n')
    result = subprocess.run(['cmd','/d','/c',str(d/'build.cmd')],capture_output=True,text=True)
    (d / 'build.log').write_text(result.stdout+result.stderr)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(d/'model.exe')],capture_output=True,text=True)
    assert result.returncode == 0, 'production loader/model failed (exit %s): %s' % (result.returncode,result.stderr)
    print('[shadow-presets] production loader/model PASS: 113 unchanged, 12 applied', flush=True)
    return json.loads(result.stdout)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', default='.tmp/rig_shadow')
    args = ap.parse_args()
    os.chdir(ROOT)
    out = (ROOT / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    oldtext = subprocess.check_output(['git','show',BASE+':'+XML],text=True,encoding='utf-8')
    baseline = llsd(ET.fromstring(oldtext))['presets']
    library = llsd(ET.parse(ROOT / XML).getroot())
    presets = library['presets']
    assert len(baseline)==113 and len(presets)==125
    assert presets[:113] == baseline, 'existing preset changed'
    assert len({p['name'].casefold() for p in presets}) == len(presets)
    for p in presets[113:]:
        assert p['name'].removeprefix(PREFIX) in FAMILIES
        assert p['category']=='Shadow Sculpt' and len(p['lights'])==4
        for l in p['lights']:
            assert l['fixture_mode'] and l['flicker_idx']==0 and l['flicker_amount']==0
            assert l['gobo_idx']==0 and l['gel_idx']==0
            assert .01<=l['source_size_m']<=4
    defaults = settings_defaults()
    assert defaults['RenderSpotShadowDetailBias'] is True, 'cold-default activation'
    from rig_shadow_review_check import check as review_check
    assert review_check(out)['passed'], 'review source contracts'
    pipeline=(ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    assert 'const bool world_owned_pick = s_idx >= 0 && worldChannelOwnsPickedLight(volume->getID());' in pipeline
    assert 'shader.uniform1i(LLShaderMgr::PROJECTOR_SHADOW_INDEX, world_owned_pick ? -1 : s_idx);' in pipeline
    assert 'ALWorldShadow::worldEligibleWithNative(native_sampled,picked)' in pipeline
    frames = compile_model(out, presets, baseline, defaults)
    from rig_shadow_gl import evaluate
    result = evaluate(presets[113:],frames,defaults,out)
    from rig_shadow_flags_check import check as check_flags
    from rig_shadow_detail_check import evaluate as evaluate_detail
    result['world_flag_checks'] = check_flags(out)
    result['detail_checks'] = evaluate_detail(defaults,out)
    result['failures'] += ['detail: '+failure for failure in result['detail_checks']['failures']]
    result['baseline'] = BASE
    result['schema_limits'] = [
        'Bundled rows cannot author bounce globals; selection resets bounce ON at 0.45.',
        'category is reduced to Genre/Mood vs standard by the loader; name prefix supplies visible grouping.',
        'Soft-shadow LOOK/quality/master settings are external; evaluated at shipped defaults with Relight toggled.',
    ]
    result['passed'] = not result['failures']
    (out/'results.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print('[shadow-presets]', 'PASS' if result['passed'] else 'FAIL', result['failures'], flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    os.environ['PYTHONUTF8'] = '1'
    sys.exit(main())
