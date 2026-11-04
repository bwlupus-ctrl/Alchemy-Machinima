#!/usr/bin/env python3
"""Variable ND gates against the PRODUCTION on-lens shader, hidden WGL.

No viewer build or login. Baseline modules come from git blobs, never a checkout.
Uses the same WGL/float-target runner as the existing sensor post fixtures.
Writes metrics, before/after PNGs and a contact sheet in .tmp/vnd_20261103.
"""
from __future__ import annotations

import argparse
import ctypes as C
from functools import lru_cache
import hashlib
import itertools
import json
import xml.etree.ElementTree as ET
import math
import os
from pathlib import Path
import subprocess
import sys
import time

os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import sensor_post_gl_check as P
import variable_nd_mean_table as M

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/vnd_20261103'
SHADER = 'class1/alchemy/onLensFiltersF.glsl'
MODULES = P.common_modules() + ('class1/alchemy/postEffectUtilsF.glsl',)
BASE = '99b1e8cf747'
LUMA = np.array([.2126, .7152, .0722])
PRESETS = [('Clean Pro', 0, 3.), ('Budget', 1, 5.), ('Pushed Too Far', 2, 8.)]
MEAN_TABLE = M.read_table()


@lru_cache(maxsize=512)
def reference_lens(fov, aspect, angle=0., radial_power=.25):
    """Independent old 16K quadrature, used to validate the new host lookup."""
    tangent = math.tan(math.radians(fov) / 2)
    coord = (np.arange(128) + .5) * (2 / 128) - 1
    x, y = np.meshgrid(coord * tangent * aspect, coord * tangent)
    corner = math.atan(math.hypot(aspect, 1) * tangent)
    radial = (np.arctan(np.hypot(x, y)) / corner) ** radial_power
    axis = math.atan(1 / aspect)
    phi = np.arctan2(y, x) - math.radians(angle)
    folded = np.arctan2(abs(np.sin(phi)), abs(np.cos(phi)))
    sector = np.where(folded < axis, axis, math.pi/2 - axis)
    leak = abs(np.sin((folded - axis) * math.pi/2 / sector)) ** .85
    return tangent, aspect, float((radial * leak).sum() / radial.sum())


def lens(fov, aspect, angle=0., radial_power=.25):
    if radial_power != .25:  # Isolated theta^4 negative control.
        return reference_lens(fov, aspect, angle, radial_power)
    tangent = math.tan(math.radians(fov) / 2)
    return tangent, aspect, M.lookup(MEAN_TABLE, tangent, aspect, math.radians(angle))


def pollution_blend(quality, stops, enabled=True):
    blend = (.012, .06, .18)[quality] * max((stops - 4) / 4, 0.) ** 2 if enabled else 0.
    return blend if blend >= .002 else 0.


def sun_uniform(u, v, z=1.):
    """Host ramp reference, after projection to full-sensor UV."""
    def ramp(lo, hi, value):
        t = max(0., min(1., (value-lo)/(hi-lo)))
        return t*t*(3-2*t)
    outside = max(-u, u-1, -v, v-1)
    weight = (1-ramp(.25, .75, outside))*ramp(0., .25, z)
    return max(-.75, min(1.75, u)), max(-.75, min(1.75, v)), weight, 0.


def scene(w=256, h=144, grey=False, all_sky=False):
    y, x = np.mgrid[:h, :w]
    image = np.ones((h, w, 4), np.float32)
    image[..., :3] = .18
    depth = np.ones_like(image) * .7
    albedo = image.copy()
    if not grey:
        # Scene radiance is proportional to material albedo, including gentle
        # spatial texture, so the real classifier's confidence is meaningful.
        image[..., :3] = (.045, .19, .35)
        depth[y < h * .62] = .7
        depth[y >= h * .62] = 1.
        image[(y < h * .62) & (x < w * .44), :3] = (.08, .25, .055)  # leaves
        image[(y < h * .62) & (x >= w * .44) & (x < w * .66), :3] = (.025, .027, .030)  # cloth
        image[(y < h * .62) & (x >= w * .66), :3] = (.40, .22, .14)  # skin
        texture = .94 + .06 * np.sin(x * .37) * np.cos(y * .29)
        image[..., :3] *= texture[..., None]
        albedo = image.copy()
    if all_sky:
        depth[:] = 1.
    image[..., 3] = (x % 19) / 19  # exact alpha preservation, not just alpha=1
    albedo[..., 3] = 0.
    orm = np.ones_like(image)
    orm[..., :3] = (1., .85, 0.)
    orm[..., 3] = .15
    normal = np.zeros_like(image)
    normal[..., :3] = (.5, .5, 1.)
    normal[..., 3] = .67  # production PBR flag
    return image, depth, albedo, orm, normal


class Fixture:
    def __init__(self):
        OUT.mkdir(parents=True, exist_ok=True)
        self.r = P.Runner()

    def program(self, no_ir=False, reference=None, mutation=None):
        return self.r.program(SHADER, '#define ON_LENS_NO_IR 1\n' if no_ir else '',
                              MODULES, ref=reference, mutation=mutation)

    def render(self, inputs, quality=0, stops=3., fov=50., angle=0., enabled=True,
               no_ir=None, extra=None, mutation=None, reference=None, fmt=0x8814, omit=(), draw_size=None):
        image, depth, albedo, orm, normal = inputs
        h, w = image.shape[:2]
        if draw_size is not None:
            w, h = draw_size
        pollution = pollution_blend(quality, stops, enabled)
        if no_ir is None:
            no_ir = pollution == 0. and not (extra or {}).get('on_lens_ir', (0.,))[0]
        if no_ir:
            pollution = 0.
        pr = self.program(no_ir, reference, mutation)
        tangent, aspect, mean = lens(fov, w / h, angle)
        uniforms = dict(exposure=1., vnd_params=(float(enabled), stops, float(quality), math.radians(angle)),
            vnd_lens=(tangent, aspect, mean, pollution), vnd_frame=(1., 1., 0., 0.),
            vnd_draw_size=(float(w), float(h), float(w), float(h)), vnd_ir=(1., .5, .4, math.sqrt(2)),
            gradnd_params=(0., 0., .5, .35), gradnd_params2=(0., 0., .6, 0.),
            gradnd_sun=(.2, .85, 1., aspect), polarizer_params=(0., 1., 0., .5),
            polarizer_params2=(0., 3., .98, 0.), night_mask_params=(0., 3., 1., .8),
            night_mask_params2=(0., 0., 0., 0.), night_mask_shape=0,
            night_mask_anchor_view=(0., 0., -2.), night_mask_tint=(1., 1., 1.),
            on_lens_grads=(1., 1., 1., 0.), on_lens_grads2=(1., 1., 1., 0.),
            on_lens_ir=(0., 0., 1., .1), on_lens_ir2=(1., .5, .4, 2.),
            on_lens_ir3=(0., math.sqrt(2), .4, .25), on_lens_ir4=(.03, 0., 1.15, 0.),
            on_lens_ir5=(0., 0., 0., 0.), on_lens_ir_up=(0., 1., 0., -10.))
        uniforms.update(extra or {})
        uniforms.setdefault("vnd_sun", uniforms["gradnd_sun"])
        for name in omit:
            uniforms.pop(name, None)
        self.r.gl.use(pr)
        matrix = np.eye(4, dtype=np.float32)
        self.r.gl.matrix(self.r.gl.loc(pr, b'night_mask_inv_proj'), 1, 0,
                         matrix.ctypes.data_as(C.POINTER(C.c_float)))
        textures = [(name, self.r.upload('vnd-' + name, data, fmt=fmt)) for name, data in (
            ('diffuseRect', image), ('depthMap', depth), ('onLensAlbedoMap', albedo),
            ('specularRect', orm), ('normalMap', normal),
            ('exposureMap', np.ones((1, 1, 4), np.float32)))]
        dst = self.r.target('vnd-out', w, h, fmt=fmt)
        return self.r.draw(pr, dst, w, h, textures, uniforms)[0]


def exposure_metrics(before, after):
    b, a = before[..., :3] @ LUMA, after[..., :3] @ LUMA
    h, w = b.shape
    centre = (slice(h//2, h//2+1), slice(w//2, w//2+1))
    return dict(mean=float(abs(a.mean() / max(b.mean(), 1e-9) - 1)),
                centre=float(abs(a[centre].mean() / max(b[centre].mean(), 1e-9) - 1)),
                centre_stops=float(abs(math.log2(max(a[centre].mean(), 1e-9) / max(b[centre].mean(), 1e-9)))),
                min_gain=float((a / np.maximum(b, 1e-9)).min()),
                max_gain=float((a / np.maximum(b, 1e-9)).max()),
                maximum=float(a.max()))


def keep_exposure(metrics):
    return metrics['mean'] <= .03 and metrics['centre_stops'] <= 1.


def cross_contrast(image, angle=0., central=False):
    """Stops = log2(wedge-band mean Y / arm-band mean Y).

    Square-pixel azimuth: arms within +/-5 degrees of the two physical lines
    aimed at the frame corners at ring=0; wedges within +/-5 degrees of the
    horizontal/vertical bisectors. Both use radius .35-.95 frame half-heights
    (a complete annulus at 16:9). The centre check instead uses ALL radii in
    the central 40% width x 40% height rectangle. Ring rotates both masks.
    """
    h, w = image.shape[:2]
    y, x = np.mgrid[:h, :w]
    x = (x + .5 - w/2) / (h/2)
    y = (y + .5 - h/2) / (h/2)
    radius = np.hypot(x, y)
    phi = np.arctan2(y, x) - math.radians(angle)
    phi = np.arctan2(abs(np.sin(phi)), abs(np.cos(phi)))
    region = ((abs(x) <= .4*w/h) & (abs(y) <= .4) if central else
              (radius > .35) & (radius < .95))
    diagonal = region & (abs(phi - math.atan(h/w)) <= math.radians(5))
    wedges = region & ((phi <= math.radians(5)) | (phi >= math.radians(85)))
    assert diagonal.sum() > 20 and wedges.sum() > 20, 'insufficient band samples'
    lum = image[..., :3] @ LUMA
    return float(math.log2(lum[wedges].mean() / lum[diagonal].mean()))


def cast_delta_e(image):
    """CIEDE2000, D65 linear sRGB, mean chroma after removing spatial gain.

    Normalize each grey pixel to Y=.18 so the measurement cannot confuse the
    X's brightness variation with the luminance-preserving colour cast.
    """
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter('ignore')
        import colour
    matrix = colour.RGB_COLOURSPACES['sRGB'].matrix_RGB_to_XYZ
    rgb = image[..., :3].astype(np.float64)
    rgb *= (.18 / np.maximum(rgb @ LUMA, 1e-9))[..., None]
    lab = colour.XYZ_to_Lab(rgb.mean(axis=(0, 1)) @ matrix.T)
    neutral = colour.XYZ_to_Lab(np.full(3, .18) @ matrix.T)
    return dict(delta_e_2000=float(colour.delta_E(neutral, lab, method='CIE 2000')),
                lab=lab.tolist(), neutral_lab=neutral.tolist())


def png(path, pixels):
    # Fixed display transform for ALL images. No per-image auto-normalisation.
    rgb = np.maximum(pixels[::-1, :, :3], 0.)
    rgb = np.where(rgb <= .0031308, rgb * 12.92, 1.055 * rgb ** (1/2.4) - .055)
    im = Image.fromarray(np.uint8(np.clip(rgb, 0., 1.) * 255 + .5))
    im.save(path)
    return im


def run():
    f = Fixture()
    rows, sweep = [], []

    def check(name, passed, details=None):
        row = dict(name=name, passed=bool(passed), details=details)
        rows.append(row)
        print(('PASS ' if passed else 'FAIL ') + name +
              (': ' + json.dumps(details) if details is not None else ''), flush=True)
        (OUT / 'gates.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')

    colorful = scene()
    grey = scene(grey=True)
    # Compare the full production pass to the user-specified base, including
    # all combinations of other filters and BOTH shader permutations.
    parity = []
    for grad, pol, ir, frame, night in itertools.product((False, True), repeat=5):
        extra = dict(gradnd_params=(1.5 if grad else 0., .13, .5, .35),
            polarizer_params=(.7 if pol else 0., 1.4, .3, .5),
            polarizer_params2=(1.5 if pol else 0., 3., .98, 1.),
            on_lens_ir=(.8 if ir else 0., float(grad), 1., .1),
            on_lens_grads2=(1.1, 1., .9, .4 if frame else 0.),
            night_mask_params=(float(night), 1., 2., .8))
        for no_ir in ((False,) if ir else (False, True)):
            for fmt in (0x8814, 0x881A):  # HDR32 diagnostic + shipped HDR16
                before = f.render(colorful, enabled=False, extra=extra, no_ir=no_ir, reference=BASE, fmt=fmt)
                after = f.render(colorful, enabled=False, extra=extra, no_ir=no_ir, fmt=fmt)
                parity.append(float(abs(before-after).max()))
    check('disabled bit-identical to base for 32 filter combinations and both permutations/targets',
          max(parity) == 0., dict(max_error=max(parity), renders=len(parity)))
    # The host changes the no-material case from the old full program to no-IR.
    a = f.render(colorful, enabled=False, no_ir=False, reference=BASE)
    b = f.render(colorful, enabled=False, no_ir=True)
    check('no-material permutation matches previous default shader', np.array_equal(a.view(np.uint32), b.view(np.uint32)))
    sampler_names = (b'onLensAlbedoMap', b'specularRect', b'normalMap')
    check('no-IR permutation has no active material samplers; IR permutation has all three',
          all(f.r.gl.loc(f.program(True), name) < 0 and f.r.gl.loc(f.program(False), name) >= 0
              for name in sampler_names))

    for label, inputs in [('grey', grey), ('colorful', colorful), ('sky', scene(grey=True, all_sky=True))]:
        before = f.render(inputs, enabled=False)
        for quality, stops, fov in itertools.product(range(3), range(1, 9), (25., 50., 90., 110.)):
            after = f.render(inputs, quality, float(stops), fov)
            metric = exposure_metrics(before, after)
            sweep.append(dict(scene=label, quality=quality, stops=stops, fov=fov, **metric))
            assert np.isfinite(after).all(), (label, quality, stops, fov, 'non-finite')
            assert np.array_equal(after[..., 3], before[..., 3]), 'VND changed bloom/glow alpha'
        print('sweep ' + label + ' complete', flush=True)
    check('keep exposure for every quality/dial/FOV on grey, sky and colorful scenes',
          all(keep_exposure(row) for row in sweep),
          dict(cases=len(sweep), max_mean=max(x['mean'] for x in sweep),
               max_centre_stops=max(x['centre_stops'] for x in sweep), worst=max(sweep, key=lambda x:x['mean'])))
    check('bounded gain on every quality/dial/FOV grey probe (0.25 to 1.5)',
          all(x['min_gain'] >= .25 - 1e-5 and x['max_gain'] <= 1.50001 for x in sweep if x['scene']=='grey'),
          dict(min_gain=min(x['min_gain'] for x in sweep), max_gain=max(x['max_gain'] for x in sweep)))
    (OUT / 'exposure_sweep.json').write_text(json.dumps(sweep, indent=2), encoding='utf-8')

    rotated_sweep = []
    for w, h in ((145, 145), (145, 257), (387, 145)):
        inputs = scene(w, h)
        before = f.render(inputs, enabled=False)
        for fov, angle in itertools.product((25., 50., 90., 110.), (-90., -60., -30., 0., 30., 60., 90.)):
            after = f.render(inputs, 2, 8., fov, angle)
            rotated_sweep.append(dict(aspect=w/h, fov=fov, angle=angle, **exposure_metrics(before, after)))
    check('keep exposure at ring endpoints and intermediate angles; portrait/square/cinemascope',
          all(keep_exposure(row) for row in rotated_sweep),
          dict(cases=len(rotated_sweep), worst=max(rotated_sweep, key=lambda x:x['mean'])))
    (OUT/'ring_aspect_sweep.json').write_text(json.dumps(rotated_sweep, indent=2), encoding='utf-8')

    # Judge the shape on exact 16:9, full-resolution probes, not thumbnails.
    grey_hd = scene(1280, 720, grey=True)
    wide = f.render(grey_hd, 2, 8., 90.)
    tele = f.render(grey_hd, 2, 8., 25.)
    middle = f.render(grey_hd, 2, 8., 50.)
    rotated = f.render(grey_hd, 2, 8., 90., 45.)
    wc, tc, rc = cross_contrast(wide), cross_contrast(tele), cross_contrast(rotated, 45.)
    mc = cross_contrast(middle)
    cc = cross_contrast(wide, central=True)
    check('Cheap 8 stops: 90 degree dark X >=1.3 stops and arm/wedge ratio <=0.40',
          wc >= 1.3 and 2**(-wc) <= .40, dict(stops=wc, arm_wedge_ratio=2**(-wc)))
    check('Cheap 8 stops: 50 degree dark X >=0.6 stop', mc >= .6, dict(stops=mc))
    check('Cheap 8 stops: 25 degree X <=0.15 stop and below one fifth of wide',
          0. <= tc <= .15 and tc < wc/5, dict(stops=tc, ratio_to_wide=tc/wc))
    check('visible near centre: central 40 percent rectangle >=0.4 stop', cc >= .4, dict(stops=cc))
    check('ring rotates dark arms with square-pixel azimuth', rc > .025 and cross_contrast(rotated) < -.025,
          dict(rotated=rc, original_orientation=cross_contrast(rotated)))
    budget_contrast = {str(fov): cross_contrast(f.render(grey_hd, 1, 5., fov)) for fov in (50., 90.)}
    check('Budget 5 stops: faint visible 90 degree X 0.15-0.35 stop; 50 degree <=0.15',
          .15 <= budget_contrast['90.0'] <= .35 and 0. <= budget_contrast['50.0'] <= .15, budget_contrast)
    pro_images = {str(fov): f.render(grey_hd, 0, 3., fov) for fov in (25., 50., 90., 110.)}
    pro_contrast = {key: cross_contrast(out) for key, out in pro_images.items()}
    pro_max_pixel_stops = max(float(abs(np.log2((out[..., :3] @ LUMA)/.18)).max())
                             for out in pro_images.values())
    check('Clean Pro 3 stops: X <=0.05 stop at every tested FOV',
          max(abs(x) for x in pro_contrast.values()) <= .05 and pro_max_pixel_stops <= .05,
          dict(band_stops=pro_contrast, max_pixel_stops=pro_max_pixel_stops))
    dial_table = [[cross_contrast(f.render(grey_hd, q, float(stops), 90.)) for stops in range(1, 9)]
                  for q in range(3)]
    dial_contrast = dial_table[2]
    check('Cheap never shows less X than Budget at the same dial',
          all(c >= b - 1e-6 for b, c in zip(dial_table[1], dial_table[2])), dial_table)
    check('Cheap 90 degree dial: 1-4 <=0.1 stop, monotonic steep rise to >=1.3 at 8',
          max(abs(x) for x in dial_contrast[:4]) <= .1 and
          all(b >= a - 1e-6 for a, b in zip(dial_contrast, dial_contrast[1:])) and
          dial_contrast[-1] >= 1.3 and dial_contrast[5] > dial_contrast[4] + .1 and
          dial_contrast[7] > dial_contrast[6] + .3, dict(stops_1_through_8=dial_contrast))
    low = []
    for stops in (1., 2.):
        out = f.render(grey, 0, stops, 90.)
        low.append(float(abs((out[..., :3] @ LUMA)/.18 - 1).max()))
    check('Pro 1-2 stops spatial artifacts invisible under 0.5 percent', max(low) < .005, low)

    cast_metrics = {}
    for name, q, stops in PRESETS:
        cast_metrics[name] = cast_delta_e(f.render(grey_hd, q, stops, 90.))
    check('mid-grey cast: Pro <1.5, Budget cool 2.5-4.5, Cheap warm/brown 7-11 deltaE2000',
          cast_metrics['Clean Pro']['delta_e_2000'] < 1.5 and
          2.5 <= cast_metrics['Budget']['delta_e_2000'] <= 4.5 and cast_metrics['Budget']['lab'][2] < 0 and
          7. <= cast_metrics['Pushed Too Far']['delta_e_2000'] <= 11. and
          cast_metrics['Pushed Too Far']['lab'][1] > 0 and cast_metrics['Pushed Too Far']['lab'][2] > 0,
          cast_metrics)

    # Isolate the production colour branch. All colours, including saturated
    # primaries, black, skin and HDR, must preserve luminance rather than merely
    # normalising a grey multiplier on the CPU.
    source = f.r.source(SHADER)
    isolated = ('return vndLimitRadiance(color, tinted * gain);', 'return tinted;')
    cast_errors = []
    for q in range(3):
        before = f.render(colorful, enabled=False)
        after = f.render(colorful, q, 8., 90., mutation=isolated)
        cast_errors.append(float(abs((after[..., :3] @ LUMA) / (before[..., :3] @ LUMA) - 1).max()))
    check('cast plus leak tint plus pollution preserve per-pixel Rec.709 luminance', max(cast_errors) < .02, cast_errors)
    # Material pollution is present, but sky/skin stay protected independently
    # of the cast. Turn only the pollution uniform off, retaining other effects.
    polluted = f.render(colorful, 2, 8., 90.)
    clean = f.render(colorful, 2, 8., 90., extra={'vnd_lens': (*lens(90., colorful[0].shape[1]/colorful[0].shape[0]), 0.)})
    delta = polluted[..., :3] - clean[..., :3]
    h, w = delta.shape[:2]
    foliage = delta[:int(h*.5), :int(w*.4)].mean(axis=(0,1))
    fabric = delta[:int(h*.5), int(w*.46):int(w*.64)].mean(axis=(0,1))
    skin = float(abs(delta[:int(h*.5), int(w*.7):]).max())
    sky = float(abs(delta[int(h*.7):]).max())
    check('IR pollution warms/magentas foliage and browns dark fabric; protects skin and sky',
          foliage[0] > .001 and foliage[2] > 0. and fabric[0] > .0001 and fabric[2] < 0.
          and skin < 1e-6 and sky < 1e-6,
          dict(foliage=foliage.tolist(), fabric=fabric.tolist(), skin=skin, sky=sky))
    fallback = f.render(colorful, 2, 8., 90., no_ir=True)
    check('unavailable IR program falls back to pollution-off while preserving X and cast',
          float(abs(fallback-clean).max()) < 1e-6,
          dict(max_error=float(abs(fallback-clean).max())))

    # Fresh GL program: the IR filter uniforms have NEVER been uploaded. VND
    # gets only its fixed response, exactly as the host's default params object.
    ir_uniforms = tuple('on_lens_ir' + suffix for suffix in ('', '2', '3', '4', '5', '_up'))
    fresh_polluted = f.render(colorful, 2, 8., 90., no_ir=False, omit=ir_uniforms,
        mutation=('// Variable ND: artifacts', '// Fresh IR-off regression. Variable ND: artifacts'))
    tuned_polluted = f.render(colorful, 2, 8., 90., no_ir=False,
        extra={'on_lens_ir2': (16., 0., 1., 8.), 'on_lens_ir3': (8., 16., 1., 1.)})
    check('IR off and never-touched uniforms: fixed-default foliage/fabric drift independent of IR tuning',
          np.array_equal(fresh_polluted, polluted) and np.array_equal(tuned_polluted, polluted)
          and foliage[0] > .001 and fabric[0] > .0001,
          dict(fresh_max_error=float(abs(fresh_polluted-polluted).max()),
               tuned_max_error=float(abs(tuned_polluted-polluted).max())))

    # Exercise every draw with a different frustum and ring on the same resident
    # program, including return to earlier values and quality/dial changes.
    changing_frames = []
    for i in range(36):
        w, h = ((256,144), (144,256), (336,144))[i % 3]
        fov, angle, q, stops = (25.,50.,90.,110.)[i % 4], (-60.,0.,30.,60.)[i%4], (i//3)%3, 5.+i%4
        inputs = scene(w, h, grey=True)
        out = f.render(inputs, q, stops, fov, angle)
        changing_frames.append(dict(frame=i, fov=fov, aspect=w/h, angle=angle,
                                    **exposure_metrics(inputs[0], out)))
    check('FOV aspect CrossAngle quality and stops change each frame: mean within 3 percent on EVERY draw',
          all(x['mean'] <= .03 for x in changing_frames),
          dict(frames=len(changing_frames), worst=max(changing_frames, key=lambda x:x['mean'])))
    invalid_means = [f.render(grey, 2, 8., 90., extra={'vnd_lens': (1.,16/9,m,.18)})
                     for m in (0., -1., float('nan'))]
    check('not-ready zero negative or NaN bias is exact VND identity',
          all(np.array_equal(out, grey[0]) for out in invalid_means))

    call = 'diff.rgb = applyVariableND(diff.rgb, vary_fragcoord, exposure_scale);'
    nonfinite = scene(grey=True)
    nonfinite[0][..., :3] = (float('inf'), 1., 2.)
    inf_out = f.render(nonfinite, mutation=(call, 'diff.rgb = vndLimitRadiance(diff.rgb, diff.rgb * 1.5);'))
    check('radiance guard preserves existing Inf without creating NaN',
          np.array_equal(inf_out, nonfinite[0]), dict(nans=int(np.isnan(inf_out).sum())))
    signed = scene(grey=True)
    signed[0][:, :128, :3] = (-1., 0., 0.)
    signed[0][:, 128:, :3] = (0., 0., 0.)
    signed_out = f.render(signed, mutation=(call, 'diff.rgb = vndKeepLuma(diff.rgb, vec3(1.0));'))
    check('negative and zero luma pass through colour normalization unchanged', np.array_equal(signed_out, signed[0]))

    horizon = scene(256, 144)
    horizon[0][..., :3] = horizon[2][..., :3] = (.08, .25, .055)
    depths = np.linspace(.99989, 1., 256, dtype=np.float32)
    horizon[1][...] = depths[None,:,None]
    horizon_out = f.render(horizon, 2, 8., 90., mutation=(call,
        'diff.rgb = vndPollution(diff.rgb, vary_fragcoord, lensSkyDepth(texture(depthMap, vary_fragcoord).r));'))
    drift = horizon_out[0, :, 0] - horizon[0][0, :, 0]
    t = np.clip((depths.astype(np.float64)-float(np.float32(.9999))) /
                (1.-float(np.float32(.9999))), 0., 1.)
    expected = 1. - t*t*(3.-2.*t)
    # Known leaf: f=1, s=0, confidence=1. Independently calculate the smooth
    # depth fade, material gain and luminance-normalized stain on the CPU.
    leaf = np.array([.08,.25,.055])
    amount = .18 * min(.5/(leaf@LUMA)*.25, 1.) * expected
    horizon_ref = leaf * (1. + amount[:,None] * (np.array([1.9,.62,1.4])-1.))
    horizon_ref *= ((leaf@LUMA)/(horizon_ref@LUMA))[:,None]
    horizon_error = float(abs(horizon_out[0,:,:3]-horizon_ref).max())
    check('pollution fades smoothly over horizon depth interval and is zero on sky',
          np.all(np.diff(drift) <= 1e-7) and drift[-1] == 0. and
          0. < drift[np.argmin(abs(expected-.5))] < drift[0] and
          horizon_error < 1e-6,
          dict(max_adjacent_drift=float(abs(np.diff(drift)).max()), ground_drift=float(drift[0]),
               sky_drift=float(drift[-1]), smoothstep_reference_error=horizon_error))

    hot = scene(grey=True)
    hot[0][..., :3] = np.linspace(0., 64., hot[0].shape[1])[None, :, None]
    before = f.render(hot, enabled=False)
    after = f.render(hot, 2, 8., 90.)
    lum_before, lum_after = before[..., :3] @ LUMA, after[..., :3] @ LUMA
    check('finite HDR and luminance ceiling without clipping existing highlights',
          np.isfinite(after).all() and np.all(lum_after <= np.maximum(lum_before, 16.) + 1e-6 * np.maximum(lum_before, 16.))
          and np.all(lum_after <= lum_before * 1.50001 + 1e-6),
          dict(maximum=float(lum_after.max()), max_input=float(lum_before.max())))
    hot[0][..., :3] *= 1000.
    half = f.render(hot, 2, 8., 90., fmt=0x881A)
    check('shipped half-float target cannot overflow on saturated HDR input',
          np.isfinite(half).all(), dict(maximum_channel=float(half[..., :3].max())))

    # The same sky band must be confined by production hardware depth, and
    # must respond to the shared sun position rather than a painted fixed mask.
    sky_inputs = scene(grey=True, all_sky=True)
    sky_a = f.render(sky_inputs, 2, 8., 90.)
    sky_b = f.render(sky_inputs, 2, 8., 90., extra={'gradnd_sun': (.8, .2, 1., 16/9)})
    ground_a = f.render(grey, 2, 8., 90.)
    ground_b = f.render(grey, 2, 8., 90., extra={'gradnd_sun': (.8, .2, 1., 16/9)})
    sky_difference = float(abs((sky_a[..., :3]-sky_b[..., :3]) @ LUMA).max())
    check('uneven sky band responds to sun and spares foreground by depth',
          sky_difference > .0001 and np.array_equal(ground_a, ground_b), dict(sky_difference=sky_difference))
    # Isolate the sky term from the approved X and cast. This is the actual
    # production band, with base spatial gain set to identity by a test mutation.
    flat_gain = ('float gain = clamp(1.0 + relative_leak * radial * (leak - bias), 0.25, 1.5);',
                 'float gain = 1.0;')
    sky_only = f.render(scene(1280,720,grey=True,all_sky=True), 2, 8., 90., mutation=flat_gain)
    sky_stops = -np.log2((sky_only[...,:3]@LUMA)/.18)
    check('Cheap 8 FOV90 sky band visible at 0.2-0.3 stop with balanced full-sky mean',
          .2 <= float(sky_stops.max()) <= .3 and abs(float((sky_only[...,:3]@LUMA).mean()/.18-1)) < .001,
          dict(max_darken_stops=float(sky_stops.max()), mean_shift=float((sky_only[...,:3]@LUMA).mean()/.18-1)))

    allocated = scene(512,288,grey=True)
    allocated[0][...,3] = 1.
    drawn = scene(128,72,grey=True)
    drawn[0][...,3] = 1.
    allocation_result = f.render(allocated, 2, 8., 90., no_ir=True, draw_size=(128,72))
    draw_result = f.render(drawn, 2, 8., 90., no_ir=True)
    check('actual draw-size uniform makes X independent of larger scene allocation',
          np.array_equal(allocation_result, draw_result))

    # Tile UV is promoted into full-frame sensor space, so the X must join
    # exactly across a high-resolution snapshot. Isolate sky/material effects
    # by using flat sky. The full-frame sun must agree across all four tiles.
    # Compare to a single full-sensor production draw.
    tile_w, tile_h = 128, 72
    full = scene(tile_w*2, tile_h*2, grey=True, all_sky=True)
    full_result = f.render(full, 2, 8., 90., 23., no_ir=True)
    assembled = np.zeros_like(full_result)
    full_lens = (*lens(90., tile_w/tile_h, 23.), 0.)
    for tx, ty in itertools.product(range(2), repeat=2):
        extra = {'vnd_frame': (.5, .5, tx*.5, ty*.5), 'vnd_draw_size': (tile_w,tile_h,tile_w*2,tile_h*2),
                 'vnd_lens': full_lens, 'vnd_sun': (.2, .85, 1., 0.)}
        ys, xs = slice(ty*tile_h, (ty+1)*tile_h), slice(tx*tile_w, (tx+1)*tile_w)
        inputs = tuple(a[ys, xs].copy() for a in full)
        assembled[ys, xs] = f.render(inputs, 2, 8., 90., 23., no_ir=True, extra=extra)
    tile_error = float(abs(assembled-full_result).max())
    check('snapshot X and sky use one full-frame optical centre without tile seams', tile_error < 1e-6, dict(max_error=tile_error))

    normal_after_tiles = f.render(full, 2, 8., 90., 23., no_ir=True)
    zero_frame = f.render(full, 2, 8., 90., 23., no_ir=True, extra={'vnd_frame': (0.,0.,.5,.5)})
    check('normal draw after tiles uploads identity; zero frame scale also means identity',
          np.array_equal(normal_after_tiles, full_result) and np.array_equal(zero_frame, full_result))

    crop_errors, symmetry_errors = [], []
    for cw, ch, crop_x, crop_y in ((224,96,8,19), (96,224,19,8)):
        tw, th = ((160,90) if cw > ch else (90,160))
        virtual_zoom = 1.5
        tangent = math.tan(math.radians(90.)/2) * ch/(th*virtual_zoom)
        crop_fov = math.degrees(math.atan(tangent))*2
        aspect = cw/ch
        params = (*lens(crop_fov,aspect), 0.)
        whole = scene(cw,ch,grey=True)
        expected_crop = f.render(whole, 2, 8., crop_fov, no_ir=True)
        stitched = np.zeros_like(expected_crop)
        for tx, ty in itertools.product(range(2), repeat=2):
            ox, oy = tx*tw-crop_x, ty*th-crop_y
            tile = f.render(scene(tw,th,grey=True), 2, 8., crop_fov, no_ir=True,
                extra={'vnd_frame':(tw/cw,th/ch,ox/cw,oy/ch), 'vnd_draw_size':(tw,th,cw,ch), 'vnd_lens':params})
            x0,y0,x1,y1 = max(ox,0),max(oy,0),min(ox+tw,cw),min(oy+th,ch)
            if x1 > x0 and y1 > y0:
                stitched[y0:y1,x0:x1] = tile[y0-oy:y1-oy,x0-ox:x1-ox]
        crop_errors.append(float(abs(stitched[...,:3]-expected_crop[...,:3]).max()))
        symmetry_errors.append(float(abs(expected_crop[...,:3]-expected_crop[::-1,::-1,:3]).max()))
        # Both projection axes recover the cropped frustum, not window aspect.
        py = virtual_zoom / math.tan(math.radians(90.)/2)
        px = py/(tw/th)
        assert abs(1/(py*th/ch)-tangent) < 1e-12
        assert abs((py*th/ch)/(px*tw/cw)-aspect) < 1e-12
    check('fractional 1.5x zoom portrait and non-window crop: one symmetric full-frame X',
          max(crop_errors) < 1e-6 and max(symmetry_errors) < 1e-5,
          dict(max_errors=crop_errors, half_turn_symmetry_errors=symmetry_errors))

    # Capture window != world-view rect, with crop offsets and dynamic render
    # scaling. Scope data must follow the former; a world-view-derived tile is
    # an explicit negative control. Adjacent centres differ by one render texel.
    edge_errors, wrong_rect_errors = [], []
    for capture, world, draw, crop, offset in (
            ((1920,1080),(1600,900),(960,540),(2800,1500),(40,31)),
            ((1080,1920),(900,1700),(540,960),(1500,2800),(31,40)),
            ((160,90),(136,76),(160,90),(224,96),(8,19))):
        for axis in (0, 1):
            tile, rendered, full_size = capture[axis], draw[axis], crop[axis]
            texels = full_size * rendered/tile
            first_origin = -offset[axis]*rendered/tile
            next_origin = (tile-offset[axis])*rendered/tile
            left = (rendered-.5+first_origin)/texels
            right = (.5+next_origin)/texels
            edge_errors.append(abs((right-left)-1/texels))
            wrong_rect_errors.append(abs((.5+(world[axis]-offset[axis])*rendered/tile)/texels-right))
    check('tile-edge continuity uses capture window even when world rect differs, including dynamic resolution',
          max(edge_errors) < 1e-12 and min(wrong_rect_errors) > .01,
          dict(max_edge_error=max(edge_errors), wrong_world_rect_min_error=min(wrong_rect_errors)))

    # Mutation tests exercise the SAME predicates above, not special thresholds.
    bad_global = f.render(grey, 2, 8., 90., mutation=('return vndLimitRadiance(color, tinted * gain);', 'return vndLimitRadiance(color, tinted * gain) * exp2(-stops);'))
    metric = exposure_metrics(f.render(grey, enabled=False), bad_global)
    check('negative control: global ND attenuation FAILS keep exposure', not keep_exposure(metric), metric)
    ignore_fov = ('vnd_lens.x', '1.0')
    bad_wide = f.render(grey_hd, 2, 8., 90., mutation=ignore_fov)
    bad_tele = f.render(grey_hd, 2, 8., 25., mutation=ignore_fov)
    bw, bt = cross_contrast(bad_wide), cross_contrast(bad_tele)
    check('negative control: ignoring FOV FAILS telephoto predicate', not (0. <= bt <= .15 and bt < bw/5), dict(wide=bw, tele=bt))
    axes = ('float axis = atan(1.0 / max(vnd_lens.y, 1e-4));\n'
            '    float folded = atan(abs(sin(phi - vnd_params.w)), abs(cos(phi - vnd_params.w)));',
            'float axis = 0.78539816339;\n'
            '    float folded = atan(abs(sin(phi - vnd_params.w - 0.78539816339)), abs(cos(phi - vnd_params.w - 0.78539816339)));')
    bad_axes = f.render(grey_hd, 2, 8., 90., mutation=axes)
    check('negative control: axes instead of diagonals FAILS geometry', not (cross_contrast(bad_axes) >= 1.3),
          dict(contrast=cross_contrast(bad_axes)))
    theta4_only = ('float radial = pow(clamp(theta / max(corner, 1e-4), 0.0, 1.0), 0.25);',
                   'float radial = pow(clamp(theta / max(corner, 1e-4), 0.0, 1.0), 4.0);')
    bad_centre = cross_contrast(f.render(grey_hd, 2, 8., 90., mutation=theta4_only,
        extra={'vnd_lens': (*lens(90., 16/9, 0., 4.), .18)}), central=True)
    check('negative control: theta^4-only FAILS visible near centre predicate',
          not (bad_centre >= .4), dict(central_stops=bad_centre))

    # Check source wiring in addition to GLSL output: the fixture cannot prove
    # the viewer uploads a uniform or chooses the correct resident permutation.
    pipeline = (ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    block = pipeline[pipeline.index('void LLPipeline::applyOnLensFilters'):pipeline.index('void LLPipeline::generateGlow')]
    check('host: VND participates in enable gates and after-capture lens draw',
          block.count('!vnd.enabled') == 2 and '|| grads_active || vnd.enabled)' in block
          and 'looks && vnd.enabled ? 1.f : 0.f' in block)
    check('host: VND pollution shares IR material bindings and saved inverse projection',
          'if (looks && material_active)' in block and '(night_active || material_active)' in block
          and 'include_gradnd_pol && material_active' in block
          and 'gOnLensFiltersProgram : gOnLensFiltersNoIRProgram' in block)
    check('host: actual saved FOV and full-frame snapshot coordinates',
          'gGLLastProjection[5]' in block and 'vnd_scale_y' in block and
          'LLRenderPass::actorFxFragOffset()' in block and 'gGLLastProjection[0]' in block and
          'vnd_offset = snapshot->offset' in block and 'tile_width = snapshot->tile.mV[VX]' in block)
    host = (ROOT/'indra/newview/allenslooks.cpp').read_text(encoding='utf-8')
    cache = host[host.index('F32 ALLensLooks::variableNDMeanLeak'):host.index('void ALLensLooks::resolveFrame')]
    check('host: synchronous constant-cost table with invalid-input identity before any draw',
          all(key in cache for key in ('VND_MEAN[ti + dt][ai + da]', 'log2f(aspect)',
              'remainderf(angle_rad', 'return 0.f;')) and
          all(key not in cache for key in ('for (', 'F64 ', 'U32 quality', 'F32 stops')) and
          block.index('ALLensLooks::variableNDMeanLeak') < block.index('const auto draw_on_lens_pass'))
    check('host: every non-tiled draw explicitly resets frame; draw size comes from actual scratch viewport',
          'if (!gSnapshot)\n                on_lens_program.uniform4f(s_vnd_frame, 1.f, 1.f, 0.f, 0.f);' in block and
          'uniform4f(s_vnd_draw_size, (F32)mWaterDis.getWidth(), (F32)mWaterDis.getHeight()' in block)
    pollution_source = source[source.index('vec3 vndPollution'):source.index('vec3 applyVariableND')]
    row_source = (ROOT/'indra/newview/alkitrow.cpp').read_text(encoding='utf-8')
    snapshot_source = (ROOT/'indra/newview/llviewerwindow.cpp').read_text(encoding='utf-8')
    check('host: independent VND default IR response, all material inputs, fallback capability annotation, scoped snapshot crop',
          'const ALLensLooks::InfraredParams vnd_ir_defaults;' in block and
          all('vnd_ir_defaults.'+field in block for field in ('foliage_gain','sensitivity','dry','skin_gain')) and
          'on_lens_ir2' not in pollution_source and 'on_lens_ir3' not in pollution_source and
          all('bindTexture(LLShaderMgr::'+name in block for name in ('ON_LENS_ALBEDO','DEFERRED_SPECULAR','NORMAL_MAP')) and
          'gOnLensNoIR ? 0.f : IR_BLEND' in host and 'Material pollution unavailable: IR shader unsupported.' in row_source and
          'VariableNDSnapshotScope vnd_snapshot' in snapshot_source)

    # The resolver's gOnLensNoIR gate is useful, but selection must be safe
    # independently even with enabled IR or nonzero pollution passed in.
    selections = []
    for no_ir, complete, enabled, strength, blend, looks in itertools.product((False, True), repeat=6):
        ready = not no_ir and complete
        ir_active = ready and enabled and strength
        material_active = ready and (ir_active or blend)
        use_material = looks and material_active
        ir_upload = bool(looks and ir_active)
        pollution_upload = bool(looks and ready and blend)
        selections.append((not use_material or (complete and not no_ir)) and
                          (use_material or (not ir_upload and not pollution_upload)) and
                          (not ready or ir_upload == bool(looks and enabled and strength)))
    check('host: all 64 fallback selections avoid unloaded IR and zero both IR/pollution uploads',
          all(selections) and all(s in block for s in (
              'material_ready = !gOnLensNoIR && gOnLensFiltersProgram.isComplete()',
              'ir_active = material_ready && ir.enabled && ir.strength > 0.f',
              'material_active = material_ready && (ir_active || vnd.ir_blend > 0.f)',
              'looks&&ir_active?ir.strength:0.f',
              'looks && material_ready ? vnd.ir_blend : 0.f')),
          dict(simulated_combinations=len(selections)))
    check('host/shader: sub-LSB pollution is zero, tested before material fetches, shares fetched sky',
          pollution_blend(0, 4.25) == 0. and pollution_blend(1, 4.25) == 0. and
          pollution_blend(2, 8.) == .18 and 'if (p.ir_blend < .002f) p.ir_blend = 0.f;' in host and
          pollution_source.index('if (vnd_lens.w <= 0.0) return color;') < pollution_source.index('texture(onLensAlbedoMap') and
          'depthMap' not in pollution_source and '1.0 - sky' in pollution_source and
          'vndPollution(tinted, uv, sky)' in source)
    off_inputs = scene(64, 36)
    tiny_blend = f.render(off_inputs, 0, 4.25)
    no_blend = f.render(off_inputs, 0, 4.25, no_ir=False,
        extra={'vnd_lens': (*lens(50., 64/36), 0.)})
    check('sub-LSB dial uses NoIR and matches capable program with zero pollution',
          np.array_equal(tiny_blend, no_blend))

    # Off-grid accuracy and timing with every input changing (no cache hits).
    rng = np.random.default_rng(3003)
    errors = []
    for fov, aspect, angle in zip(rng.uniform(1., 170., 768), np.exp2(rng.uniform(-4., 4., 768)), rng.uniform(-90., 90., 768)):
        t, a, reference = reference_lens(float(fov), float(aspect), float(angle))
        errors.append(abs(M.lookup(MEAN_TABLE, t, a, math.radians(angle)) - reference))
    check('host table off-grid accuracy vs old 16K quadrature across 1:16..16:1 sensors',
          max(errors) < .004 and len(MEAN_TABLE) == 17*65*65 and
          all(0. < value < 1. for value in MEAN_TABLE),
          dict(samples=len(errors), max_absolute_bias_error=max(errors), table_bytes=len(MEAN_TABLE)*4))
    timings = []
    for batch in range(20):
        start = time.perf_counter_ns()
        for i in range(1000):
            M.lookup(MEAN_TABLE, .2+(batch*1000+i)*.0001, 1.+i*.001, i*.0001)
        timings.append((time.perf_counter_ns()-start)/1e6/1000)
    check('host lookup reference changing-frustum cost below 0.05 ms per recompute', max(timings) < .05,
          dict(mean_ms=float(np.mean(timings)), max_batch_ms=max(timings), recomputes=20000,
               measurement='CPython scalar equivalent, includes interpreter overhead; viewer native timing not measured'))

    # Exercise both ends of each visibility margin with small pan/tilt steps,
    # plus a directional sun crossing z=0. Real production GLSL on all-sky grey.
    small_sky = scene(48, 48, grey=True, all_sky=True)
    sun_steps = []
    for coordinate in ('u', 'v'):
        for lo, hi in ((-.8, -.2), (1.2, 1.8)):
            previous = None
            for value in np.linspace(lo, hi, 1201):  # 0.0005 UV steps
                u, v = (value, .85) if coordinate == 'u' else (.2, value)
                result = f.render(small_sky, 2, 8., 90., no_ir=True,
                                  extra={'vnd_sun': sun_uniform(u, v)})
                if previous is not None:
                    sun_steps.append(float(abs(result[...,:3]-previous[...,:3]).max()))
                previous = result
    previous = None
    for z in np.linspace(-.15, .15, 601):
        divisor = max(z, .001)
        result = f.render(small_sky, 2, 8., 90., no_ir=True,
            extra={'vnd_sun': sun_uniform(.45/divisor+.5, .15/divisor+.5, z)})
        if previous is not None:
            sun_steps.append(float(abs(result[...,:3]-previous[...,:3]).max()))
        previous = result
    # Also isolate the front ramp with fixed projected coordinates. An ordinary
    # perspective pan leaves the UV margin before z=0; this probe makes the
    # front factor itself observable rather than letting the margin hide it.
    previous = None
    for z in np.linspace(-.05, .3, 701):
        result = f.render(small_sky, 2, 8., 90., no_ir=True,
                          extra={'vnd_sun': sun_uniform(.2, .85, z)})
        if previous is not None:
            sun_steps.append(float(abs(result[...,:3]-previous[...,:3]).max()))
        previous = result
    check('sun visibility and behind-camera sweeps stay continuous, max RGB step <0.003',
          max(sun_steps) < .003 and all(s in block for s in (
              '1.f - smooth_ramp(.25f, .75f, outside)', 'smooth_ramp(0.f, .25f, vnd_sun_clip.z)',
              'sun_divisor = llmax(vnd_sun_clip.z, .001f)', 'llclamp(raw_sun_v, -.75f, 1.75f)')),
          dict(steps=len(sun_steps), uv_or_z_step=.0005, max_rgb_step=max(sun_steps), bound=.003))

    guarded = []
    for width, height in itertools.product((0., 1., 72., 1920.), repeat=2):
        w, h = max(width, 1.), max(height, 1.)
        guarded.extend((1./w, 1./h, 512./w, 288./h, 17./w, 23./h))
    check('host: zero/minimized sizes are guarded before every sensor division',
          all(math.isfinite(x) for x in guarded) and all(
              block.index(f'vnd_size.mV[{axis}] = llmax(vnd_size.mV[{axis}], 1.f);') < block.index('const F32 vnd_scale_x')
              for axis in ('VX', 'VY')))
    check('capture: exact readback tile dimensions and lower-left offsets supplied by rawSnapshot',
          all(s in snapshot_source for s in (
              '(F32)window_width, (F32)window_height,',
              '(F32)(subimage_x * window_width - buffer_x_offset)',
              '(F32)(subimage_y * window_height - buffer_y_offset)')) and
          'tile_height = snapshot->tile.mV[VY]' in block)
    settings = ET.parse(ROOT/'indra/newview/app_settings/settings_alchemy.xml').getroot().find('map')
    pairs = list(settings)
    declarations = {pairs[i].text: pairs[i+1] for i in range(0, len(pairs), 2)}
    keys = ['RenderVariableND'+suffix for suffix in ('Enabled', 'Preset', 'Stops', 'Quality', 'CrossAngle')]
    def setting_fields(name):
        fields = list(declarations[name])
        return {fields[i].text: fields[i+1].text for i in range(0, len(fields), 2)}
    check('all five settings persist with declared types and defaults',
          all(setting_fields(name)['Persist']=='1' for name in keys) and
          [setting_fields(name)['Type'] for name in keys] == ['Boolean','U32','F32','U32','F32'] and
          [setting_fields(name)['Value'] for name in keys] == ['0','0','3','0','0'])
    check('fixed VND response matches IR settings defaults rather than zero or user values',
          [float(setting_fields('RenderInfrared'+suffix)['Value']) for suffix in
              ('FoliageEV','FoliageSensitivity','DryVegetation','SkinEV')] == [0.,.5,.4,.5])
    page = ET.parse(ROOT/'indra/newview/skins/default/xui/en/panel_dpage_lens_filters.xml').getroot()
    card = next(c for c in page.iter('kit_card') if c.get('id')=='variable_nd')
    controls = list(card.iter('kit_row'))
    search = (ROOT/'indra/newview/skins/default/xui/en/director_kit_index.xml').read_text(encoding='utf-8')
    vnd_hits = [line.split('\t') for line in search.splitlines() if '\tvariable_nd\t' in line]
    check('Director: five searchable controls, help, reset and value read-back',
          len(vnd_hits)==5 and all('vnd variable nd fader x cross cross polarization colour cast ir pollution' in hit[14]
                                  for hit in vnd_hits) and
          card.get('enable_setting')==keys[0] and bool(card.get('enable_tooltip')) and
          {c.get('setting') for c in controls}==set(keys[1:]) and
          all(c.get('reset_settings')==c.get('setting') and c.get('help') for c in controls) and
          all(c.find('slider').get('show_text')=='true' for c in controls if c.get('kind')=='slider'))

    renders = OUT/'renders'
    renders.mkdir(exist_ok=True)
    # Every individual image is full 1280x720. The overview uses 640x360
    # previews: two rows per preset (grey/color), off/on at each of 50/90 FOV.
    thumb_w, thumb_h, row_h = 640, 360, 406
    sheet = Image.new('RGB', (4*thumb_w, 8*row_h), '#191b20')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 23)
    hd_scenes = [('grey', grey_hd), ('colour', scene(1280, 720))]
    render_metrics = []

    def preview(im, col, row, label):
        x, y = col*thumb_w, row*row_h
        draw.text((x+10, y+9), label, font=font, fill='white')
        sheet.paste(im.resize((thumb_w, thumb_h), Image.Resampling.LANCZOS), (x, y+40))

    for preset_row, (name, q, stops) in enumerate(PRESETS):
        for scene_row, (scene_name, inputs) in enumerate(hd_scenes):
            for fov_col, fov in enumerate((50., 90.)):
                before = f.render(inputs, enabled=False, fov=fov)
                after = f.render(inputs, q, stops, fov)
                slug = name.lower().replace(' ', '_') + '_' + str(int(fov))
                a = png(renders/(slug+'_'+scene_name+'_before.png'), before)
                b = png(renders/(slug+'_'+scene_name+'_after.png'), after)
                # Keep established artifact names pointing to the new HD images.
                if scene_name == 'grey':
                    b.save(renders/(slug+'_grey.png'))
                else:
                    a.save(renders/(slug+'_before.png'))
                    b.save(renders/(slug+'_after.png'))
                row = preset_row*2 + scene_row
                preview(a, fov_col*2, row, f'{name} | {scene_name} | FOV {int(fov)} | OFF')
                preview(b, fov_col*2+1, row, f'{name} | {scene_name} | FOV {int(fov)} | ON')
                render_metrics.append(dict(preset=name, scene=scene_name, fov=fov,
                                           **exposure_metrics(before, after)))
    for col, stops in enumerate((2., 4., 6., 8.)):
        im = png(renders/f'cheap_dial_{int(stops)}_90_grey.png', f.render(grey_hd, 2, stops, 90.))
        preview(im, col, 6, f'Cheap dial | {int(stops)} stops | grey | FOV 90')
    for col, (scene_name, inputs) in enumerate(hd_scenes):
        for offset, angle in enumerate((0., 30.)):
            result = f.render(inputs, 2, 8., 90., angle)
            im = png(renders/f'pushed_too_far_90_{scene_name}_angle_{int(angle)}.png',
                     result)
            preview(im, col*2+offset, 7, f'Pushed Too Far | {scene_name} | CrossAngle {int(angle)}')
            render_metrics.append(dict(preset='Pushed Too Far', scene=scene_name, fov=90., angle=angle,
                **exposure_metrics(f.render(inputs, enabled=False), result)))
    sheet.save(renders/'contact_sheet.png')
    (OUT/'render_metrics.json').write_text(json.dumps(render_metrics, indent=2), encoding='utf-8')
    check('1280x720 preset/FOV grey+colour renders keep mean exposure and centre within one stop',
          all(keep_exposure(x) for x in render_metrics),
          dict(cases=len(render_metrics), max_mean=max(x['mean'] for x in render_metrics)))
    check('saved HD comparisons, Cheap 2/4/6/8 strip, CrossAngle 30 and readable contact sheet',
          all(Image.open(path).size == (1280, 720) for path in renders.glob('*.png') if path.name != 'contact_sheet.png'),
          dict(individual_resolution=[1280,720], contact_sheet_resolution=list(sheet.size)))
    approved = np.array([1.3926094403, .6646353674, .0062443956, 1.2628096095,
                         .1453781252, .2850691003])
    measured = np.array([wc, mc, tc, cc, budget_contrast['50.0'], budget_contrast['90.0']])
    check('approved X strength remains within 0.05 stop of round 2',
          max(abs(measured-approved)) <= .05,
          dict(measured=measured.tolist(), max_stop_delta=float(max(abs(measured-approved)))))
    summary = dict(renderer=f.r.gl.renderer, baseline=BASE,
        shader_sha256=hashlib.sha256(source.encode()).hexdigest(),
        gates=len(rows), failed=[x['name'] for x in rows if not x['passed']],
        exposure_cases=len(sweep) + len(rotated_sweep) + len(render_metrics) + len(changing_frames),
        max_mean=max(x['mean'] for x in sweep + rotated_sweep + render_metrics + changing_frames),
        max_centre_stops=max(x['centre_stops'] for x in sweep + rotated_sweep + render_metrics + changing_frames),
        dial_table=dial_table,
        cross_stops=dict(cheap_90=wc, cheap_50=mc, cheap_25=tc, cheap_central_90=cc,
                         budget=budget_contrast, pro=pro_contrast, cheap_dial=dial_contrast),
        cast=cast_metrics, theta4_only_central_stops=bad_centre)
    (OUT/'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    return int(bool(summary['failed']))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--worker', action='store_true')
    ap.add_argument('--timeout', type=int, default=240)
    args = ap.parse_args()
    if args.worker:
        return run()
    return subprocess.run([sys.executable, __file__, '--worker'], cwd=ROOT,
                          env=dict(os.environ, PYTHONUTF8='1'), timeout=args.timeout).returncode


if __name__ == '__main__':
    raise SystemExit(main())
