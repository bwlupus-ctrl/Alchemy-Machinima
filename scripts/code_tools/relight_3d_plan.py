"""Effective soft-shadow quality plan, computed by compiling the PRODUCTION alareashadowmath.h at the shader ref.

LLPipeline::renderAreaShadows plans the march with ALAreaShadow::workloadOrUniform(): at 1080p the budget lowers
rays/resolution, so "Cinematic" is not 24 samples at full resolution in the viewer. The harness asks the real header.
If the header cannot be compiled (no MSVC, API change) a table captured from HEAD 4dd9ff661c1 is used and flagged.
"""
import hashlib
import json
import os
import subprocess
from pathlib import Path
from typing import NamedTuple

from relight_3d_gl import REPO

CONFIGS = [('Default', 1, False), ('Default+Thin', 1, True), ('Cinematic', 3, False), ('Cinematic+Thin', 3, True)]

# Captured with the real header at HEAD 4dd9ff661c1 for 1920x1080, budget 1500, requested lights 4, no feed slots.
# key: name -> list indexed by (lights-1): (samples, steps, half, near_steps)
FALLBACK = [{"preset": 1, "thin": 0, "lights": 1, "samples": 8, "steps": 12, "half": 1, "near": 0, "channels": 4}, {"preset": 1, "thin": 0, "lights": 2, "samples": 8, "steps": 12, "half": 1, "near": 0, "channels": 4}, {"preset": 1, "thin": 0, "lights": 3, "samples": 8, "steps": 12, "half": 1, "near": 0, "channels": 4}, {"preset": 1, "thin": 0, "lights": 4, "samples": 8, "steps": 12, "half": 1, "near": 0, "channels": 4}, {"preset": 1, "thin": 1, "lights": 1, "samples": 8, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 1, "thin": 1, "lights": 2, "samples": 8, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 1, "thin": 1, "lights": 3, "samples": 8, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 1, "thin": 1, "lights": 4, "samples": 8, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 3, "thin": 0, "lights": 1, "samples": 5, "steps": 32, "half": 0, "near": 0, "channels": 2}, {"preset": 3, "thin": 0, "lights": 2, "samples": 5, "steps": 32, "half": 0, "near": 0, "channels": 2}, {"preset": 3, "thin": 0, "lights": 3, "samples": 5, "steps": 32, "half": 0, "near": 0, "channels": 2}, {"preset": 3, "thin": 0, "lights": 4, "samples": 5, "steps": 32, "half": 0, "near": 0, "channels": 2}, {"preset": 3, "thin": 1, "lights": 1, "samples": 9, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 3, "thin": 1, "lights": 2, "samples": 9, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 3, "thin": 1, "lights": 3, "samples": 9, "steps": 80, "half": 1, "near": 48, "channels": 2}, {"preset": 3, "thin": 1, "lights": 4, "samples": 9, "steps": 80, "half": 1, "near": 48, "channels": 2}]

CPP = r'''
#include "alareashadowmath.h"
#include <algorithm>
#include <cstdio>
int main() {
    const int presets[2] = {1, 3};
    std::printf("[");
    bool first = true;
    for (int pi = 0; pi < 2; ++pi) for (int thin = 0; thin < 2; ++thin) for (int lights = 1; lights <= 4; ++lights) {
        const int preset = presets[pi];
        const ALAreaShadow::Quality uniform = ALAreaShadow::quality(preset);
        const ALAreaShadow::Quality requested = thin ? ALAreaShadow::segmented(uniform, preset) : uniform;
        const auto w = ALAreaShadow::workloadOrUniform(requested, uniform, 1920, 1080, lights, 1500, 4, 0);
        // The blur radius LLPipeline::renderAreaShadows hands the filter (RenderAreaShadowDenoise 2, 1080p), from the
        // header's own policy when it has one (ALAreaShadow::filterRadius), else the inline formula of that commit.
        const float radius = RADIUS_EXPR;
        std::printf("%s{\"preset\":%d,\"thin\":%d,\"lights\":%d,\"samples\":%d,\"steps\":%d,\"half\":%d,\"near\":%d,\"channels\":%u,\"iter\":%llu,\"radius\":%g}",
            first ? "" : ",", preset, thin, lights, w.q.samples, w.q.steps, w.q.half ? 1 : 0, w.q.nearSteps,
            w.channels, (unsigned long long)w.iterations, (double)radius);
        first = false;
    }
    std::printf("]");
    return 0;
}
'''


def _vcvars():
    for base in (os.environ.get('ProgramFiles', r'C:\Program Files'), os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')):
        found = sorted((Path(base) / 'Microsoft Visual Studio').glob('*/*/VC/Auxiliary/Build/vcvars64.bat'))
        if found:
            return found[-1]
    return None


class PlanResult(NamedTuple):
    """Structured outcome of the planner adapter. compiled is True only when the production header produced the table
    (compiled now, or loaded from a cache that only a successful compile writes); False means the captured HEAD table."""
    plans: dict
    source: str
    compiled: bool
    error: str


def require_compiled(result):
    """Gates that claim the PRODUCTION plan call this: the captured fallback table is not the production planner."""
    if not result.compiled:
        raise AssertionError('production planner did not compile (%s); the captured HEAD table is not accepted' % (result.error or 'fallback table'))
    return result


def compute_plans(tree, work_dir, width=1920, height=1080):
    """Returns ({config name: {lights: plan dict}}, source string). width/height: the target the planner is asked about.

    Legacy two-value form (other harnesses tolerate the fallback table and print its source label); a gate that needs the
    production planner uses compute_plans_status() + require_compiled()."""
    result = compute_plans_status(tree, work_dir, width, height)
    return result.plans, result.source


def compute_plans_status(tree, work_dir, width=1920, height=1080):
    """compute_plans with a structured PlanResult (compiled flag + error text)."""
    header = tree.read('indra/newview/alareashadowmath.h')
    # The production radius policy when the header defines one, else the formula inlined in pipeline.cpp at that commit.
    radius_expr = ('ALAreaShadow::filterRadius(2.f, 1.f, w.q.half)' if 'filterRadius(' in header else
                   'std::min(8.f, std::max(0.f, 2.f * 1.f * (w.q.half ? .5f : 1.f)))')
    cpp = CPP.replace('RADIUS_EXPR', radius_expr)
    if (width, height) != (1920, 1080):
        cpp = cpp.replace('1920, 1080', '%d, %d' % (width, height)).replace('filterRadius(2.f, 1.f,', 'filterRadius(2.f, %.4ff,' % (height / 1080.0))
    key = hashlib.sha1((header + cpp).encode()).hexdigest()[:12]
    d = Path(work_dir) / 'plan' / key
    cache = d / 'plans.json'
    rows = None
    compiled, error = True, ''
    source = 'compiled alareashadowmath.h @%s' % tree.commit()
    if cache.exists():
        rows = json.loads(cache.read_text())      # written only after a successful compile of this exact header + adapter
    else:
        try:
            vc = _vcvars()
            if vc is None:
                raise RuntimeError('MSVC not found')
            d.mkdir(parents=True, exist_ok=True)
            (d / 'alareashadowmath.h').write_text(header, encoding='utf-8')
            (d / 'plan.cpp').write_text(cpp, encoding='utf-8')
            (d / 'build.cmd').write_text('@echo off\ncall "%s" >nul\ncl /nologo /std:c++17 /EHsc /I"%s" /Fo"%s" /Fe"%s" "%s"\nexit /b %%ERRORLEVEL%%\n'
                                         % (vc, d, d / 'plan.obj', d / 'plan.exe', d / 'plan.cpp'), encoding='utf-8')
            subprocess.run(['cmd', '/d', '/c', str(d / 'build.cmd')], check=True, capture_output=True)
            rows = json.loads(subprocess.check_output([str(d / 'plan.exe')], text=True))
            cache.write_text(json.dumps(rows))
        except Exception as exc:  # pragma: no cover - environment dependent
            print('[ref3d] WARNING: could not compile production planner (%s); using captured HEAD table' % exc, flush=True)
            rows = FALLBACK
            compiled, error = False, str(exc)
            source = 'FALLBACK table captured at HEAD 4dd9ff661c1 (header not compiled)'
    plans = {}
    for name, preset, thin in CONFIGS:
        plans[name] = {}
        for r in rows:
            if r['preset'] == preset and bool(r['thin']) == thin:
                # 'radius' (trace pixels) comes from the tree's own policy; the captured HEAD table predates it (HEAD: 1 at 1080p half).
                plans[name][r['lights']] = {'samples': r['samples'], 'steps': r['steps'], 'half': bool(r['half']),
                                            'near_steps': r['near'], 'channels': r['channels'],
                                            'radius': r.get('radius', 1.0 if r['half'] else 2.0)}
    return PlanResult(plans, source, compiled, error)
