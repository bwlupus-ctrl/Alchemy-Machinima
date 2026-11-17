"""WP2 production fit contract, all original samples, caps and honest verdicts."""
from pathlib import Path
import subprocess
from keyframe_core_native_check import production_run
from native_check_tools import check_directory

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'


def check():
    directory = check_directory('keyframe_fit')
    files = ('alkeyframecore.cpp', 'alkeyframefit.cpp')
    originals = {f: (NV / f).read_text(encoding='utf-8') for f in files}
    fixture = (NV / 'tests/alkeyframefit_native.cpp').read_text(encoding='utf-8')
    clean = production_run(directory, originals, fixture)
    print(clean.stdout + clean.stderr, end='', flush=True)
    assert clean.returncode == 0
    mutations = [
        ('hold_first_pair_only', 'drift', 'WP2 drift hold rejected', 'i<=last;', 'i<=std::min(last,first+1);'),
        ('uniform_60Hz_timing', 'timing', 'WP2 exact timing', 'samples[indices[j+1]].time-samples[end].time', 'double(indices[j+1]-end)/60'),
        ('skip_120Hz_originals', 'rate120', 'WP2 all 120Hz originals', 'for(std::size_t i=0;i<count;++i)\n        {\n            if(i%32', 'for(std::size_t i=0;i<count;i+=2)\n        {\n            if(i%32'),
        ('false_PASS_at_cap', 'honesty', 'WP2 APPROXIMATE honesty', 'result.verdict=FitVerdict::Approximate;result.reason="APPROXIMATE: tolerance unmet', 'result.verdict=FitVerdict::Pass;result.reason="APPROXIMATE: tolerance unmet')]
    for name, selected, assertion, before, after in mutations:
        mutated = dict(originals)
        assert mutated[files[1]].count(before) == 1, (name, before)
        mutated[files[1]] = mutated[files[1]].replace(before, after)
        exe = production_run(directory, mutated, fixture, name=name, run=False)
        failed = subprocess.run([str(exe), selected], cwd=ROOT, capture_output=True, text=True)
        output = failed.stdout + failed.stderr
        assert failed.returncode == 1 and ('FAIL ' + assertion + '\n') in output, (name, failed.returncode, output)
        assert output.count('FAIL ') == 1
        print('PASS negative [exec] ' + name + ': ' + output.strip(), flush=True)
    clean = production_run(directory, originals, fixture, name='clean_after_negatives')
    print(clean.stdout + clean.stderr, end='', flush=True)
    assert clean.returncode == 0
    print('PASS clean WP2 after 4 semantic negatives')


if __name__ == '__main__':
    check()
