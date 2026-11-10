"""Compile the production sway model, numerical tests and setup LLSD loader.

Supplement to the viewer's TUT target; no viewer build. Uses the same tests
120-122 and 124-127 from alcinelightrigmodel_test.cpp and the preset-gate LLSD
shim. Also exercises production scene/cue/instance recovery and heap guards.
PYTHONUTF8=1 python scripts/code_tools/rig_sway_check.py
"""
import argparse
import os
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from rig_shadow_presets_check import SHIM, cpp_data, function, llsd
from rig_sway_recovery import caller_fixtures, extended_shim

ROOT = Path(__file__).resolve().parents[2]


ALLOCATION_GUARD = r'''
#include <cstdlib>
#include <new>
static bool gTrackAllocations = false;
static size_t gAllocationCount = 0;
void* operator new(size_t size) {
    if (gTrackAllocations) ++gAllocationCount;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', default='.tmp/rig_sway_20261027/model')
    args = parser.parse_args()
    out = (ROOT / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdtypes.h').write_text(
        '#pragma once\n#include <cstdint>\nusing F32=float;using F64=double;'
        'using S32=int;using U32=uint32_t;using U64=uint64_t;using S64=int64_t;using U8=uint8_t;\n')
    (out / 'linden_common.h').write_text('#include "stdtypes.h"\n')
    controller = (ROOT / 'indra/newview/alcinelightrig.cpp').read_text(encoding='utf-8')
    tests = (ROOT / 'indra/newview/tests/alcinelightrigmodel_test.cpp').read_text(encoding='utf-8')
    lines = [extended_shim(SHIM), '#include <limits>\n#include <stdexcept>\n#include <boost/signals2.hpp>',
             'void set_test_name(const char* name){std::cout<<name<<"\\n";}',
             'void ensure(const char* name,bool ok){if(!ok)throw std::runtime_error(name);}',
             'template<class A,class B>void ensure_equals(const char* name,A a,B b){ensure(name,a==b);}']
    for name in ('profile', 'beam', 'gobo', 'gel', 'fixtureGel', 'fixturePreset', 'flickerProgram'):
        lines.append(function(controller, f'S32 {name}Index(const std::string& name)'))
    for signature in ('bool swayMixFromLLSD(const LLSD& mix, LightBase& light)',
                      'bool setupFromLLSD(const LLSD& data, Setup& output)',
                      'LLSD lightToLLSD(const LightBase& light)'):
        lines.append(function(controller, signature))
    lines.extend(caller_fixtures(ROOT, controller))
    lines.append(function(controller, 'void copyLights(const LightBase source[LIGHT_COUNT],'))
    handoff_start = controller.index('        copyLights(fx_base, fx_setup.mLights);')
    handoff_end = controller.index('        mCurrentRadius = setup.mRadius;', handoff_start)
    lines.append(ALLOCATION_GUARD)
    lines.append('void testNoAllocations(){ Setup setup, fx_setup; Transforms transforms;'
                 'LightBase fx_base[4], desired[4], mCurrentLive[4], mTransitionTarget[4];'
                 'LightBase a,b;a.mSwayAmount=b.mSwayAmount=.5f;'
                 'a.mSwayPeriod=2.4f;b.mSwayPeriod=4.f;'
                 'LightBase full_start,full_target;'
                 'full_start.mSwayTermCount=full_target.mSwayTermCount=4;'
                 'full_start.mSwayAmount=full_target.mSwayAmount=.7f;'
                 'for(int i=0;i<4;++i){full_start.mSwayTerms[i]={.7f,1.f+.3f*i,.25f};'
                 'full_target.mSwayTerms[i]={.7f,4.f+.3f*i,.25f};}'
                 'gTrackAllocations=true;'
                 'for(int enabled=0;enabled<2;++enabled){'
                 'a.mSwayAmount=b.mSwayAmount=enabled?.5f:0.f;'
                 'setup.mLights[0]=blendLight(a,b,.4f);')
    lines.append('for(int step=0;step<1000;++step){'
                 'evalFX(0,12345,step/60.0,fx_base);'
                 'auto mixed=blendLight(setup.mLights[0],b,.3f);'
                 'setup.mLights[0]=mixed;'
                 'if(enabled)setup.mLights[1]=blendLight(full_start,full_target,(step%100)/99.f);')
    # The FX handoff is compiled verbatim, including computeLive and both runtime copies.
    lines.append(controller[handoff_start:handoff_end])
    lines.append('RigFrame frame;render(setup.mRadius,mCurrentLive,{},frame);'
                 'F32 offset[3];evalLightSway(mCurrentLive[0],12345,step/60.0,.56f,offset);}'
                 '}gTrackAllocations=false;assert(gAllocationCount==0);'
                 'std::cout<<"PASS: zero heap allocations in 1000 off + 1000 active computeLive/blend/FX handoff/render updates\\n";}')
    for number in (120, 121, 122, 124, 125, 126, 127, 134, 135, 136):
        signature = f'void cine_light_rig_model_object::test<{number}>()'
        lines.append(function(tests, signature).replace(signature, f'void test_{number}()'))
    library = llsd(ET.parse(ROOT / 'indra/newview/app_settings/cine_light_rig_presets.xml').getroot())
    presets = library['presets']
    assert len(presets) == 128 and presets[-1]['name'] == 'Swinging Bulb'
    prior = llsd(ET.fromstring(subprocess.check_output(
        ['git', 'show', '398f7592e05:indra/newview/app_settings/cine_light_rig_presets.xml'],
        cwd=ROOT, text=True, encoding='utf-8', timeout=30)))['presets']
    assert len(prior) == 127 and presets[:127] == prior, 'existing preset changed'
    bulb = presets[-1]
    assert bulb['category'] == 'Practical' and bulb['radius'] == 1.6 and bulb['master_ev'] == .5
    assert len(bulb['lights']) == 4
    for index, light in enumerate(bulb['lights']):
        assert light['sway_amount'] == (.45 if index == 0 else 0)
        assert light['sway_period'] == 2.4 and light['on'] == (index < 2)
    assert bulb['lights'][0]['detail_shadows'] and bulb['lights'][0]['fixture_mode']
    assert bulb['lights'][0]['fixture_preset_idx'] == 10
    assert bulb['lights'][0]['kelvin'] == 2700 and bulb['lights'][0]['source_size_m'] == .05
    assert bulb['lights'][0]['yaw'] == 0 and bulb['lights'][0]['pitch'] == 70
    assert bulb['lights'][0]['ev'] == 0 and bulb['lights'][0]['beam_idx'] == 0
    assert bulb['lights'][1]['yaw'] == -40 and bulb['lights'][1]['pitch'] == 10
    assert bulb['lights'][1]['ev'] == -4 and bulb['lights'][1]['beam_idx'] == 1
    lines.append('int main(){test_120();test_121();test_122();test_124();test_125();test_126();test_127();test_134();test_135();test_136();testNoAllocations();LLSD authored;')
    cpp_data(bulb, 'authored', lines)
    lines.append('testRecovery(authored);')
    lines.append('''
    Setup original; assert(setupFromLLSD(authored,original));
    assert(original.mLights[0].mDetailShadows && original.mLights[0].mFixtureMode);
    assert(original.mLights[0].mFixturePreset==10);
    LLSD saved=authored;
    for(int i=0;i<4;++i)saved["lights"].values[i]=lightToLLSD(original.mLights[i]);
    Setup loaded;assert(setupFromLLSD(saved,loaded));
    assert(loaded.mRadius==original.mRadius && loaded.mRatioLock==original.mRatioLock &&
           loaded.mRatioStops==original.mRatioStops);
    for(int i=0;i<4;++i){
        assert(std::memcmp(static_cast<const LightValues*>(&loaded.mLights[i]),
                           static_cast<const LightValues*>(&original.mLights[i]),sizeof(LightValues))==0);
        assert(loaded.mLights[i].mSwayAmount==original.mLights[i].mSwayAmount);
        assert(loaded.mLights[i].mSwayPeriod==original.mLights[i].mSwayPeriod);
        assert(loaded.mLights[i].mDetailShadows==original.mLights[i].mDetailShadows);
        saved["lights"].values[i].fields.erase("sway_amount");
        saved["lights"].values[i].fields.erase("sway_period");
    }
    Setup legacy;assert(setupFromLLSD(saved,legacy));
    for(const auto& light:legacy.mLights){assert(light.mSwayAmount==0.f);assert(light.mSwayPeriod==2.4f);}
    LightBase second=original.mLights[0];second.mSwayPeriod=4.f;
    LightBase third=second;third.mSwayPeriod=6.f;
    const LightBase interrupted=blendLight(blendLight(original.mLights[0],second,.4f),third,.3f);
    saved["lights"].values[0]=lightToLLSD(interrupted);
    Setup restored;assert(setupFromLLSD(saved,restored));
    assert(sameSwayMix(restored.mLights[0],interrupted));
    F32 a[3],b[3];
    evalLightSway(interrupted,flickerLightSeed(12345,0),1001.0,.56f,a);
    evalLightSway(restored.mLights[0],flickerLightSeed(12345,0),1001.0,.56f,b);
    assert(std::memcmp(a,b,sizeof(a))==0);
    saved["lights"].values[0]["sway_mix"].values[0].values[2]=2.0;
    assert(setupFromLLSD(saved,restored));
    assert(restored.mLights[0].mSwayTermCount==0);
    assert(restored.mLights[0].mSwayAmount==interrupted.mSwayAmount);
    assert(restored.mLights[0].mSwayPeriod==interrupted.mSwayPeriod);
    std::cout<<"PASS: interrupted scene snapshot retains identical sway; invalid mixture recovers\\n";
    std::cout<<"PASS: production sway, transition negative control, setup round-trip and preset\\n";
    }
    #include "alcinelightrigmodel.cpp"
    ''')
    source = out / 'sway.cpp'
    source.write_text('\n'.join(lines), encoding='utf-8')
    vcvars = r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
    from native_check_tools import viewer_native_flags
    boost_includes = ' '.join(f for f in viewer_native_flags()[0] if f.startswith('/I') and 'vcpkg' in f)
    build = out / 'build.cmd'
    build.write_text(f'@echo off\ncall "{vcvars}" >nul\n'
                     f'cl /nologo /std:c++20 /utf-8 /EHsc /O2 /Ob3 /arch:AVX2 /fp:fast /W3 /WX /I"{out}" '
                     f'/I"{ROOT / "indra/newview"}" {boost_includes} "{source}" '
                     f'/Fe:"{out / "sway.exe"}" /Fo:"{out / "sway.obj"}"\nexit /b %ERRORLEVEL%\n')
    result = subprocess.run(['cmd', '/d', '/c', str(build)], cwd=out,
                            capture_output=True, text=True, timeout=120)
    (out / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(out / 'sway.exe')], cwd=out,
                            capture_output=True, text=True, timeout=30)
    (out / 'run.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    print(result.stdout, end='')
    assert result.returncode == 0, result.stderr
    # Each planted bug must fail the same executable assertions as production.
    generated = source.read_text(encoding='utf8')
    shutdown = function(controller, 'void ALCineLightRig::shutdown()')
    negatives = {
        'open_combo_baseline': ('combo && combo->isListVisible()', 'combo && false'),
        'make_coalesced': ('history.normalizeWidgetValue(mLitTarget, LLSD("make"), baseline);', '/* planted coalesced baseline */'),
        'make_left_open': ('            history.finishGesture();\n        }\n        std::string moved;', '        }\n        std::string moved;'),
        'runtime_no_retry': ('&& retry)', '&& false && retry)'),
        'runtime_unthrottled': ('const bool retry_members = member_now >= mLitMemberRetryAt;', 'const bool retry_members = true;'),
        'open_combo_selection': ('if (!mLitTarget->isListVisible())\n            setValueIfChanged(mLitTarget,', 'if (true)\n            setValueIfChanged(mLitTarget,'),
        'make_transient': ('history.normalizeWidgetValue(mLitTarget, LLSD("make"), LLSD(value));', '/* planted transient history */'),
        'make_unjoined': ('}, true, validate,', '}, false, validate,'),
        'scene_stale_names': ('setLitGroup(static_cast<Slot>(i), std::string());', '(void)i;'),
        'missing_group_refusal': ('// Missing names are authored targets: fallback now, rebind by name later.',
            'if(target.compare(0,6,"group:")==0 && LLDirectorCast::instance().membersInGroup(target.substr(6)).empty())return false;'),
        'names_per_tick': ('auto* object = gObjectList.findObject(runtime[index]);', 'auto* object = LLDirectorCast::instance().resolve(runtime[index]);'),
        'shutdown_stale': (shutdown, shutdown.replace('    mLastResolvedMembers.clear();', '')),
        'representative_snap': ('if (group_members[i]->getID() == mGroupRepresentative)', 'if (group_members[i]->getID().isNull())'),
    }
    for name, (before, after) in negatives.items():
        assert before in generated, name
        source.write_text(generated.replace(before, after, 1), encoding='utf8')
        compiled = subprocess.run(['cmd', '/d', '/c', str(build)], cwd=out, capture_output=True, text=True, timeout=120)
        assert compiled.returncode == 0, (name, compiled.stdout, compiled.stderr)
        planted = subprocess.run([str(out / 'sway.exe')], cwd=out, capture_output=True, text=True, timeout=30)
        assert planted.returncode != 0, 'planted negative unexpectedly passed: ' + name
        (out / (name + '.log')).write_text(planted.stdout + planted.stderr, encoding='utf8')
        print('PASS: rejected planted ' + name, flush=True)
    source.write_text(generated, encoding='utf8')
    from rig_lit_group_check import run_registered_unit
    run_registered_unit(ROOT)
    return 0


if __name__ == '__main__':
    os.environ['PYTHONUTF8'] = '1'
    raise SystemExit(main())
