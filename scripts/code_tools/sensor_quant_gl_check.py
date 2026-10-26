"""Noise-amount ADC regression: production GLSL, float FBO, explicit watchdog."""
import argparse
import json
import os
import subprocess
import sys

os.environ['PYTHONUTF8'] = '1'
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import numpy as np
import sensor_iso_gl_check as S


def run(reference=None):
    out = S.OUT/'quant_fix'
    out.mkdir(parents=True, exist_ok=True)
    model = S.Model(out/'model')
    runner = S.Runner()
    runner.identity_sensor = None  # Changed amounts have independent numerical oracles below.
    if reference:
        runner.sensor = subprocess.check_output(
            ['git', 'show', reference+':'+S.SHADER.relative_to(S.ROOT).as_posix()],
            cwd=S.ROOT, text=True, encoding='utf-8', timeout=15)
    rows = []
    def check(name, passed, **detail):
        rows.append(dict(name=name, passed=bool(passed), **detail))
        print(('PASS ' if passed else 'FAIL ')+name, flush=True)

    rng = np.random.default_rng(0xadc0)
    gradient = np.linspace(.06, .14, 512, dtype=np.float32)[None, :, None]
    # Existing image grain, amplitude .04; sensor amount zero must preserve it.
    target = np.broadcast_to(gradient, (64, 512, 3)).copy()
    target += rng.uniform(-.04, .04, target.shape).astype(np.float32)
    for preset in range(3):
        for iso in (100, 800, 3200, 25600):
            for bright in (0, 1):
                u, p = model.get(preset, iso, bright, amount=0.)
                # Compensate the documented Brightens gain to keep the same
                # unclipped output field in both modes; Neutral input is target.
                pixels = (target * (p['gN']/p['g'])).astype(np.float32)
                actual = runner.process_pixels(u, pixels, None, kind=14)
                error = float(np.max(abs(actual-target)))
                tolerance = 8*np.finfo(np.float32).eps
                check(f'clean gradient + .04 grain preset={preset} ISO={iso} bright={bright}',
                      error <= tolerance, maximum_error=error, tolerance=float(tolerance))

    # Exercise both R/B and individual green ADCs against an independent DN
    # oracle, including both ADC rails, at intermediate amounts.
    for amount in (0., .01, .25, .5, .75, .99, 1., 2., 4.):
        u, p = model.get(1, 3200, amount=amount)
        f = u.view(np.float32)
        f[0] = 1e6  # Isolate ADC rails from the separately tested full-well clip.
        electrons = rng.uniform(-10, p['sat']*1.5, (32, 256, 3)).astype(np.float32)
        pixels = (electrons*p['gN']/p['K']).astype(np.float32)
        pre = runner.process_pixels(u, pixels, None, kind=16)
        green = runner.process_pixels(u, pixels, None, kind=17)[:, :, :2]
        actual = runner.process_pixels(u, pixels, None, kind=15)
        sites = runner.process_pixels(u, pixels, None, kind=18)[:, :, :2]
        # In this fixture full well exceeds all collected light, so preclip
        # includes the exact noisy signal entering each ADC.
        assert electrons.max() < f[0]
        def adc(signal):
            dn = np.clip(signal*float(f[2])+float(f[3]), 0., float(f[4]))
            strength = min(amount, 1.)
            return (dn+strength*(np.rint(dn)-dn)-float(f[3]))/float(f[2])
        expected = adc(pre)
        expected_green = adc(green)
        expected[:, :, 1] = expected_green.mean(axis=2)
        # GLSL round ties may choose either neighbour. Bound the float32
        # signal first, then quantise both bounds with the SAME amount rule.
        bound = 8*np.finfo(np.float32).eps*np.maximum(abs(pre)+float(f[3])/float(f[2]), 1.)
        gb = 8*np.finfo(np.float32).eps*np.maximum(abs(green)+float(f[3])/float(f[2]), 1.)
        lo, hi = adc(pre-bound), adc(pre+bound)
        glo, ghi = adc(green-gb), adc(green+gb)
        lo[:, :, 1], hi[:, :, 1] = glo.mean(axis=2), ghi.mean(axis=2)
        decode = 8*np.finfo(np.float32).eps*np.maximum(abs(actual), 1.)
        gd = 8*np.finfo(np.float32).eps*np.maximum(abs(sites), 1.)
        error = float(max(np.max(lo-actual-decode), np.max(actual-hi-decode),
                          np.max(glo-sites-gd), np.max(sites-ghi-gd), 0.))
        check(f'continuous per-site DN oracle amount={amount}', error < 1e-4,
              maximum_error=error, nominal_error=float(np.max(abs(actual-expected))))

    if not reference:
        source, programs = runner.sensor, runner.programs
        # Compile successful mutants: each must fail the same clean-image
        # predicate, separately covering red/blue and the two green sites.
        for name, old, new in (
            ('red/blue rounding', 'vec2 dn = sensorQuantise(clamp(amplified + p.physics.w, 0.0, p.scale.x), p.controls.x);',
             'vec2 dn = round(clamp(amplified + p.physics.w, 0.0, p.scale.x));'),
            ('green rounding', 'vec2 greenDN = sensorQuantise(clamp(p.physics.z*(min(greenPhotons,vec2(p.physics.x))+greenRead) +\n                              p.physics.w, 0.0, p.scale.x), p.controls.x);',
             'vec2 greenDN = sensorQuantise(clamp(p.physics.z*(min(greenPhotons,vec2(p.physics.x))+greenRead) +\n                              p.physics.w, 0.0, p.scale.x), 1.0);'),
        ):
            assert source.count(old) == 1, name+' mutation seam'
            runner.sensor, runner.programs = source.replace(old, new), {}
            u, _ = model.get(0, 100, amount=0.)
            actual = runner.process_pixels(u, target, None, kind=14)
            error = float(np.max(abs(actual-target)))
            check('mutation rejected: '+name, error > tolerance,
                  maximum_error=error, tolerance=float(tolerance))
        runner.sensor, runner.programs = source, programs

    evidence = dict(checks=rows, renderer=runner.gl.renderer, reference=reference,
                    shader_sha256=S.hashlib.sha256(runner.sensor.encode()).hexdigest())
    (out/('before.json' if reference else 'after.json')).write_text(
        json.dumps(evidence, indent=2), encoding='utf-8')
    return int(any(not row['passed'] for row in rows))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', action='store_true')
    parser.add_argument('--reference')
    parser.add_argument('--timeout', type=int, default=240)
    args = parser.parse_args()
    if args.worker:
        raise SystemExit(run(args.reference))
    raise SystemExit(subprocess.run([sys.executable, __file__, '--worker', *sys.argv[1:]],
                                   cwd=S.ROOT, timeout=args.timeout).returncode)
