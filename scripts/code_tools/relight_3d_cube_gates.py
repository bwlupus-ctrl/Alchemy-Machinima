"""P1-lite acceptance gates over a relight_3d_reference.py results.json of the `cube` source (the brief's section 4 + the 10/23 decisions).

    py -3 -I scripts/code_tools/relight_3d_cube_gates.py .tmp/cube_r1/full3/results.json [--report .tmp/cube_r1/sheets_W/region_report.txt]

Config `cube` = the shipped defaults (N = 2 resident lights, face near plane 5 cm, emitter excluded). Gates, each a number from the harness:
  B   mae_on mean <= 0.051, worst pose <= 0.145;  A  mae_on mean <= 0.12, worst <= 0.238;  A2 (railing + receivers) mae_on mean <= 0.12, worst <= 0.238
  false_dark (acne) worst <= 0.02 and leak_on worst <= 0.05 on A, A2 and B;   C lamp pool dark <= 0.02
  camera-only flicker: pop <= 0.05 everywhere; p95 <= 0.05 on A, and <= 0.06 on B and A2 with the EXPLICIT NOTE below
  scene R and C: the harness ratchet (its own `gate_failures`) passes for `cube` at the default N = 2
  scenes W / WS / WB (with --report): wall above the lamp left lit <= 0.05 mean visibility (ground truth 0.0) and the louver MAE of the lower vent <= 0.10 for `cube`

FLICKER NOTE (user decision 10/23, the lead accepts it): the camera-only p95 of scene B (`camorbit_low`) is 0.057 against a target of 0.05 and A2's 0.053. It is MEASUREMENT-LIMITED, not
camera dependence of the world term: a grazing floor pixel is ~3 cm across against an ~8 cm penumbra, so the harness's bilinear read of pixel centres alone moves the p95; with the contact
term off the same group reads 0.048, and with a penumbra twice as wide 0.031. The target 0.05 is kept for A; B and A2 are held to 0.06, a deliberate and recorded relaxation, not a silent one.
"""
import json
import re
import sys
from pathlib import Path

LIMITS = {'B': (0.051, 0.145), 'A': (0.12, 0.238), 'A2': (0.12, 0.238)}
FLICKER_P95 = {'A': 0.05, 'B': 0.06, 'A2': 0.06}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    agg = json.loads(Path(args[0]).read_text())['agg']
    rows, fails = [], []

    def check(label, value, limit, direction='max'):
        ok = value == value and (value <= limit if direction == 'max' else value >= limit)
        rows.append('  %-4s %-58s %8.4f %s %.4f' % ('ok' if ok else 'FAIL', label, value, '<=' if direction == 'max' else '>=', limit))
        if not ok:
            fails.append(label)

    for scene in ('B', 'A', 'A2'):
        a = agg.get('%s|cube' % scene)
        if not a:
            continue
        mean, worst = LIMITS[scene]
        check('%s mae_on mean' % scene, a['mae_on_mean'], mean)
        check('%s mae_on worst pose' % scene, a['mae_on_worst'], worst)
        check('%s false_dark (acne) worst' % scene, a['false_dark_worst'], 0.02)
        check('%s leak_on worst' % scene, a['leak_on_worst'], 0.05)
        check('%s camera flicker p95 worst' % scene, a['flicker_p95_worst'], FLICKER_P95[scene])
        check('%s camera flicker pop worst' % scene, a['flicker_pop_worst'], 0.05)
    a = agg.get('C|cube')
    if a:
        check('C lamp pool dark worst', a['pool_dark_worst'], 0.02)
        check('C false_dark worst (ratchet limit, tightest baseline)', a['false_dark_worst'], 0.0091)
    res = json.loads(Path(args[0]).read_text())
    for scene in ('R', 'C'):
        if '%s|cube' % scene in agg:
            # the harness ratchet is evaluated per scene/config inside results.json only as a count: re-derive from the summary file next to it
            summary = (Path(args[0]).parent / 'summary.txt').read_text()
            bad = [m for m in re.findall(r'^  FAIL (\S+)\s+(\S+)\s+(\S+)', summary, re.M) if m[0] == scene and m[1] == 'cube']
            rows.append('  %-4s %-58s %s' % ('FAIL' if bad else 'ok', '%s ratchet (tightest HEAD limit) for `cube`' % scene, 'failures: %s' % [m[2] for m in bad] if bad else 'none'))
            if bad:
                fails.append('%s ratchet' % scene)
    rep = [a for a in sys.argv[1:] if a.startswith('--report')]
    if rep or '--report' in sys.argv:
        path = Path(sys.argv[sys.argv.index('--report') + 1])
        text = path.read_text()
        for scene in re.findall(r'=== scene (\w+):', text):
            block = text.split('=== scene %s:' % scene)[1].split('=== scene')[0]
            m = re.search(r'region wall_above.*?\n\s+cube\s+\d+\s+(\S+)\s+(\S+)\s+(\S+)', block, re.S)
            if m:
                check('%s wall above the lamp: mean visibility (GT %s)' % (scene, m.group(1)), float(m.group(2)), 0.05)
            m = re.search(r'region vent \(.*?\n\s+cube\s+\d+\s+(\S+)\s+(\S+)\s+(\S+)', block, re.S)
            if m:
                check('%s lower vent louver MAE' % scene, float(m.group(3)), 0.10)
    print('\n'.join(rows))
    print('[cube-gates] %s (%d checks, %d failures)' % ('PASS' if not fails else 'FAIL', len(rows), len(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
