"""World shadow near-clip guard: the status-line warning for a near plane that reaches into an avatar.

A world shadow cube clips by axial depth: nothing inside the world-axis cube |x - light|inf < near casts. With
RenderAreaShadowWorldNear saved at 1 m (clamped to 1 m; the per-light emitter rule can ask for as much on its own)
every avatar within a metre of a lamp loses the shadow of the part inside. The guard changes no rendering: when a
rendering cube's near plane is past 25 cm AND reaches into an avatar's bounds, the Director's World shadows status
line starts with a sentence saying so, and the throttled [RelightQ] log line carries NEAR_CLIP_REACHES_AVATAR.

The production header (indra/newview/alworldshadowmath.h) is compiled with MSVC and run: the box / cube test, the
threshold and the sentence. Then the pipeline wiring is checked on the source, and planted regressions of the
header and of the wiring must each be rejected. No viewer build.

  python scripts/code_tools/world_shadow_near_guard_check.py
"""
import re
import sys
from pathlib import Path

from native_check_tools import check_directory, compile_run

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'

HOST = r'''
#include "alworldshadowmath.h"
#include <cstdio>
#include <limits>
static int failures = 0;
static void ensure(const char* what, bool ok) { if (!ok) { std::printf("FAIL: %s\n", what); ++failures; } }
int main()
{
    using namespace ALWorldShadow;
    const double p[3] = { 10.0, 20.0, 30.0 };
    // an avatar box 0.6 x 0.6 x 1.9 m whose nearest face is `gap` metres from the light along x
    auto box_at = [&](double gap, double lo[3], double hi[3]) {
        lo[0] = p[0] + gap; hi[0] = lo[0] + 0.6; lo[1] = p[1] - 0.3; hi[1] = p[1] + 0.3; lo[2] = p[2] - 1.0; hi[2] = p[2] + 0.9; };
    double lo[3], hi[3];
    box_at(0.5, lo, hi);
    ensure("a box 0.5 m away is inside a 1 m near cube", nearCubeReaches(lo, hi, p, 1.0));
    ensure("a box 0.5 m away is outside a 0.25 m near cube", !nearCubeReaches(lo, hi, p, 0.25));
    ensure("a box that only touches the cube face is outside (open cube)", !nearCubeReaches(lo, hi, p, 0.5));
    box_at(-0.3, lo, hi);
    ensure("a box around the light is inside any near cube", nearCubeReaches(lo, hi, p, 0.05));
    ensure("no cube, no reach", !nearCubeReaches(lo, hi, p, 0.0) && !nearCubeReaches(lo, hi, p, -1.0) &&
           !nearCubeReaches(lo, hi, p, std::numeric_limits<double>::quiet_NaN()));
    // the cube is a CUBE: a box beside its corner, farther than `near` in a straight line but inside on every axis
    lo[0] = p[0] + 0.8; hi[0] = lo[0] + 0.5; lo[1] = p[1] + 0.8; hi[1] = lo[1] + 0.5; lo[2] = p[2] + 0.8; hi[2] = lo[2] + 0.5;
    ensure("a box at the cube's corner (1.39 m from the light) is inside the 1 m cube", nearCubeReaches(lo, hi, p, 1.0));
    lo[2] = p[2] + 1.01; hi[2] = lo[2] + 0.5;
    ensure("one axis outside is outside", !nearCubeReaches(lo, hi, p, 1.0));
    // each axis and each side
    for (int axis = 0; axis < 3; ++axis)
        for (int side = -1; side <= 1; side += 2)
        {
            for (int i = 0; i < 3; ++i) { lo[i] = p[i] - 0.1; hi[i] = p[i] + 0.1; }
            lo[axis] = p[axis] + side * 0.7 - 0.1; hi[axis] = p[axis] + side * 0.7 + 0.1;
            ensure("axis/side inside", nearCubeReaches(lo, hi, p, 0.65));
            ensure("axis/side outside", !nearCubeReaches(lo, hi, p, 0.55));
        }
    ensure("threshold is 25 cm", kNearClipWarn == 0.25);
    ensure("the 5 cm default never warns", !nearClipWarns(0.05) && !nearClipWarns(0.25));
    ensure("26 cm and 1 m warn", nearClipWarns(0.26) && nearClipWarns(1.0));
    ensure("not a number does not warn", !nearClipWarns(std::numeric_limits<double>::quiet_NaN()));
    const std::string note = nearClipNote(1.0, "light 2d5b6e9d");
    ensure("the sentence names the distance, the light and the remedy",
           note == "Near clip 1.00 m reaches an avatar at light 2d5b6e9d: the part inside casts no world shadow. Lower Near clip.");
    // the runtime clamp the guard relies on: the setting never asks for more than 1 m, the emitter rule neither
    ensure("setting clamp", nearPlane(1.05) == 1.0 && emitterNearPlane(1.05, 0.0, 10.0) == 1.0 && emitterNearPlane(0.05, 0.6, 10.0) > 0.6);
    std::printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
'''

HEADER_NEGATIVES = {
    'sphere_instead_of_cube': ('        if (!(lo[i] < p[i] + nearPlane && hi[i] > p[i] - nearPlane)) return false;\n    return true;',
                               '        if (!(lo[i] < p[i] + nearPlane && hi[i] > p[i] - nearPlane)) return false;\n'
                               '    double q = 0.0;\n    for (int i = 0; i < 3; ++i) { const double c = std::max(lo[i] - p[i], std::max(p[i] - hi[i], 0.0)); q += c * c; }\n'
                               '    return q < nearPlane * nearPlane;'),
    'one_sided_test': ('lo[i] < p[i] + nearPlane && hi[i] > p[i] - nearPlane', 'lo[i] < p[i] + nearPlane'),
    'closed_cube': ('lo[i] < p[i] + nearPlane && hi[i] > p[i] - nearPlane', 'lo[i] <= p[i] + nearPlane && hi[i] >= p[i] - nearPlane'),
    'zero_near_reaches': ('    if (!(nearPlane > 0.0)) return false;\n', ''),
    'default_near_warns': ('constexpr double kNearClipWarn = 0.25;', 'constexpr double kNearClipWarn = 0.02;'),
    'warn_threshold_inclusive': ('nearPlane > kNearClipWarn; }', 'nearPlane >= kNearClipWarn; }'),
    'sentence_without_remedy': (' Lower Near clip.";', '";'),
}

COMPUTE = 'if (!ALWorldShadow::nearClipWarns(jobs[j].near_plane)) continue;'
REACH = 'if (ALWorldShadow::nearCubeReaches(lo, hi, p, jobs[j].near_plane))'
STORE = 'sWorldShadowNearCut[jobs[j].slot] = jobs[j].near_plane;'
RESET = 'sWorldShadowNearCut.fill(0.f);   // [Near clip guard] restated below by every frame that renders cubes'
READ = 'if (reasons[k] == ALWorldShadow::SlotReason::Rendered && sWorldShadowNearCut[k] > 0.f)'
NOTE = 'near_note = ALWorldShadow::nearClipNote(sWorldShadowNearCut[k], labels[k]) + " ";'
RETURN = 'return near_note + ALWorldShadow::worldShadowReport('
LOG = 'text += " NEAR_CLIP_REACHES_AVATAR";'

PIPELINE_NEGATIVES = {
    'no_reset_each_frame': (RESET, ''),
    'warning_never_shown': (RETURN, 'return ALWorldShadow::worldShadowReport('),
    'warns_for_a_light_that_is_not_rendering': (READ, 'if (sWorldShadowNearCut[k] > 0.f)'),
    'threshold_skipped': (COMPUTE, ''),
    'stored_by_job_index': (STORE, 'sWorldShadowNearCut[j] = jobs[j].near_plane;'),
    'log_verdict_missing': (LOG, ''),
    'near_plane_clamped_instead': ('jobs[job_count++] = {k, found, found->getPositionAgent(), radius, worldShadowLightNear(found, radius)};',
                                   'jobs[job_count++] = {k, found, found->getPositionAgent(), radius, llmin(worldShadowLightNear(found, radius), 0.25f)};'),
}


def function(text, signature):
    start = text.index(signature)
    depth, i = 0, text.index('{', start)
    while True:
        depth += text[i] == '{'
        depth -= text[i] == '}'
        if depth == 0:
            return text[start:i + 1]
        i += 1


def pipeline_contracts(text):
    gen = function(text, 'void LLPipeline::generateWorldShadowCubes(LLCamera& camera)')
    readout = function(text, 'std::string LLPipeline::worldShadowReadout() const')
    first = gen.index('{') + 1
    return {
        'reset_is_the_first_statement': gen[first:].lstrip().startswith(RESET),
        'computed_once_after_the_jobs': gen.count(COMPUTE) == 1 and gen.count(REACH) == 1 and gen.count(STORE) == 1 and
            gen.index('jobs[job_count++] = {k, found, found->getPositionAgent(), radius, worldShadowLightNear(found, radius)};') < gen.index(COMPUTE) and
            gen.index(COMPUTE) < gen.index(REACH) < gen.index(STORE),
        'avatars_by_animated_extents': 'const LLVector3* ext = avatar->getLastAnimExtents();' in gen[gen.index(COMPUTE):gen.index(STORE)] and
            'if (!avatar || avatar->isDead()) continue;' in gen[gen.index(COMPUTE):gen.index(STORE)],
        'rendering_is_untouched': 'llmin(worldShadowLightNear' not in text and 'sWorldShadowNearCut' not in text.split('ALWorldShadow::emitterNearPlane(')[1].split('static F32 worldShadowLightNear')[0],
        'readout_puts_it_first_for_rendering_lights_only': readout.count(READ) == 1 and readout.count(NOTE) == 1 and readout.count(RETURN) == 1 and
            readout.index(READ) < readout.index(NOTE) < readout.index(RETURN),
        'log_carries_the_verdict': gen.count(LOG) == 1 and 'if (sWorldShadowNearCut[js[j].slot] > 0.f)' in gen,
        'state_written_in_one_function': text.count('sWorldShadowNearCut[') == 4 and text.count('sWorldShadowNearCut.fill(') == 1,
    }


def run_host(directory, header_text, name):
    (directory / 'alworldshadowmath.h').write_text(header_text, encoding='utf-8')
    for dependency in re.findall(r'#include "(al[^"]+\.h)"', header_text):
        (directory / dependency).write_text((NV / dependency).read_text(encoding='utf-8'), encoding='utf-8')
    return compile_run(directory, HOST, name=name, flags=[f'/I"{directory}"'])


def main():
    directory = check_directory('world_shadow_near_guard')
    header = (NV / 'alworldshadowmath.h').read_text(encoding='utf-8')
    pipeline = (NV / 'pipeline.cpp').read_text(encoding='utf-8')
    failures = []
    result = run_host(directory, header, 'near_guard')
    print(result.stdout.strip())
    if result.returncode or 'OK' not in result.stdout:
        failures.append('production header: ' + (result.stdout + result.stderr).strip()[-400:])
    contracts = pipeline_contracts(pipeline)
    for name, ok in contracts.items():
        print(('ok: ' if ok else 'FAIL: ') + 'pipeline ' + name)
        if not ok:
            failures.append('pipeline contract ' + name)
    for name, (before, after) in HEADER_NEGATIVES.items():
        assert header.count(before) == 1, ('mutation drift', name)
        try:
            bad = run_host(directory, header.replace(before, after), 'neg_' + name)
            caught = bad.returncode != 0
        except RuntimeError:
            caught = True   # does not compile
        print(('PASS rejected' if caught else 'FAIL survived') + ' planted regression: ' + name)
        if not caught:
            failures.append('planted regression survived: ' + name)
    for name, (before, after) in PIPELINE_NEGATIVES.items():
        assert pipeline.count(before) == 1, ('mutation drift', name)
        try:
            caught = not all(pipeline_contracts(pipeline.replace(before, after)).values())
        except ValueError:
            caught = True   # a statement the contracts are anchored on is gone
        print(('PASS rejected' if caught else 'FAIL survived') + ' planted regression: ' + name)
        if not caught:
            failures.append('planted regression survived: ' + name)
    print('[near-guard]', 'PASS' if not failures else 'FAIL', failures)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
