"""Run the production Extra rims header and its registered TUT tests natively.

Each mutation is compiled independently and must fail its own named assertion.
Generated sources and executables stay under this worktree on the I: drive.
"""
from pathlib import Path
import re

from native_check_tools import check_directory, compile_run, viewer_native_flags
from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]


def run_unit(root=ROOT):
    root = Path(root)
    header = (root / 'indra/newview/alextrarimmodel.h').read_text(encoding='utf8')
    tests = (root / 'indra/newview/tests/alcinelightrigmodel_test.cpp').read_text(encoding='utf8')
    registration = re.search(r'typedef test_group<cine_light_rig_model_data, (\d+)> cine_light_rig_model_group;', tests)
    assert registration and int(registration[1]) >= 141, 'extra-rim tests must be registered, not merely compiled'
    lines = [
        '#include "linden_common.h"',
        '#include "lltut.h"',
        '#include <iostream>\n#include <limits>',
        header,
        'namespace tut {',
        'test_runner_singleton runner;',
        'using namespace ALCineLightRigModel;',
        'struct cine_light_rig_model_data {};',
        registration[0],
        'typedef cine_light_rig_model_group::object cine_light_rig_model_object;',
        'cine_light_rig_model_group cine_light_rig_model_tests("ALExtraRimModel");',
    ]
    for number in range(137, 142):
        lines += ['template<> template<>', function(tests, f'void cine_light_rig_model_object::test<{number}>()')]
    lines += ['}',
        'int main(){for(int n=137;n<=141;++n){auto result=tut::cine_light_rig_model_tests.run_test(n);'
        'if(result.result!=tut::test_result::ok){std::cerr<<result.message<<"\\n";return 1;}}'
        'std::cout<<"PASS: actual TUT registration and execution of Extra rims tests 137-141\\n";return 0;}',
    ]
    source = '\n'.join(lines)
    flags, libraries = viewer_native_flags()
    flags += ['/UNDEBUG', f'/I"{root / "indra/externals/tut"}"']
    directory = check_directory('extra_rims_model')
    baseline = compile_run(directory, source, 'baseline', flags, libraries)
    (directory / 'baseline.log').write_bytes((baseline.stdout + baseline.stderr).encode('utf8'))
    assert baseline.returncode == 0, baseline.stdout + baseline.stderr
    print(baseline.stdout, end='', flush=True)
    mutations = {
        'xr_dir_mirrored': (
            'dst_centre + (src_pos - src_centre)',
            'dst_centre + (src_centre - src_pos)'),
        'xr_scale_ignored': (
            'const F32 k = effectiveRadius(src_radius, dst_scale) /\n'
            '                  effectiveRadius(src_radius, src_scale);',
            'const F32 k = 1.f;'),
        'xr_source_auto_fallback': (
            'if (!(lit_mask & bit)) return {slot, SourceDark::RIG_OFF};',
            'if (!(lit_mask & bit)) return {0, SourceDark::NONE};'),
        'xr_cap_exceeded': (
            'state.mItems.size() >= kMaxExtraRims)',
            'state.mItems.size() >= kMaxExtraRims + 1)'),
        'xr_scene_bad_trim_accepted': (
            'return std::isfinite(trim) && trim >= 0. && trim <= 2.;',
            'return true;'),
    }
    for name, (before, after) in mutations.items():
        assert source.count(before) == 1, (name, 'mutation anchor drift')
        planted = compile_run(directory, source.replace(before, after, 1), name, flags, libraries)
        output = planted.stdout + planted.stderr
        (directory / (name + '.log')).write_bytes(output.encode('utf8'))
        assert planted.returncode != 0, name + ': planted negative unexpectedly passed'
        assert name in output.splitlines(), (name, 'failed a different assertion', output)
        print('PASS: rejected planted ' + name + ' at its named TUT assertion', flush=True)


if __name__ == '__main__':
    run_unit()
