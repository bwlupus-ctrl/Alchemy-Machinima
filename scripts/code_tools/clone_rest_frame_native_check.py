"""Execute the production pure C++ helper against the Python/GL frame cases."""
import json
from pathlib import Path

from native_check_tools import check_directory, compile_run


def check():
    from clone_rest_frame_check import numeric_cases
    root = Path(__file__).resolve().parents[2]
    cases = numeric_cases()

    def literal(values):
        return '{' + ','.join(float(v).hex() + 'f' for v in values) + '}'

    rows = []
    for case in cases:
        rows.append('{const float inv[16]=' + literal(case['inv_bind_flat'])
                    + ';const float world[3]=' + literal(case['default_world'])
                    + ';const float expected[12]=' + literal(case['delta'])
                    + ';const auto actual=ALGhostRestFrame::compute(inv,world);'
                    + 'for(int i=0;i<12;++i) assert(std::abs(actual[i]-expected[i])<=2e-7f);'
                    + 'if(std::all_of(expected,expected+12,[](float v){return v==0.f;}))'
                    + 'for(float v:actual) assert(std::bit_cast<unsigned>(v)==0u);}\n')
    source = '''#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "alghostrestframe.h"
int main(){
''' + ''.join(rows) + '''
    float inv[16]={.01f,0,0,0, 0,0,.01f,0, 0,.01f,0,0, 0,0,-1.067f,1};
    float world[3]={0,0,1.067f};
    assert(ALGhostRestFrame::compute(inv,world)!=ALGhostRestFrame::Delta{});
    for(int i=0;i<16;++i){
        float bad[16];std::copy(inv,inv+16,bad);
        bad[i]=std::numeric_limits<float>::quiet_NaN();
        assert(ALGhostRestFrame::compute(bad,world)==ALGhostRestFrame::Delta{});
        bad[i]=std::numeric_limits<float>::infinity();
        assert(ALGhostRestFrame::compute(bad,world)==ALGhostRestFrame::Delta{});
    }
    for(int i=0;i<3;++i){
        float bad[3];std::copy(world,world+3,bad);
        bad[i]=std::numeric_limits<float>::infinity();
        assert(ALGhostRestFrame::compute(inv,bad)==ALGhostRestFrame::Delta{});
    }
    std::cout<<"PASS production C++ rest delta: Python/GL numeric cases including five logged custom poses and translated foreign frames, exact-zero bits, all 16 nonfinite slots, nonfinite default translations\\n";
}
'''
    directory = check_directory('clone_rest_frame')
    result = compile_run(directory, source, flags=(f'/I"{root / "indra/newview"}"', '/W4', '/WX'))
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, flush=True)
    metrics = dict(cases=[c['name'] for c in cases], max_absolute_error=2e-7,
                   exact_zero_bits=True, invalid_slots=16, exit=result.returncode)
    (directory / 'results.json').write_text(json.dumps(metrics, indent=2) + '\n', encoding='utf-8')
    return metrics


if __name__ == '__main__':
    check()
