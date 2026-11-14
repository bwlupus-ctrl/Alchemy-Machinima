"""Camera-motion flicker of the projector soft shadow on production GL (real shadowUtil.glsl).

Everything in the world is static: one lamp, one round blocker, one receiver plane, one shadow map. Only the camera
moves. It stands square on the plane, so a truck by N pixels moves every point of the plane by exactly N pixels and
the world point under a pixel of frame 0 can be read again in every later frame (bilinear between the four pixels
around it: exact for a whole-pixel truck). Every frame is rendered by the production sampler (sampleSpotShadow,
driven through rig_shadow_gl.Runner like relight_spot_metric_gl_check.py). A shadow that belongs to the world gives
the same value for the same world point in every frame, so the measurement is

    flicker = mean over the penumbra's world points of mean |v[k] - v[k-1]| * 255     (8-bit steps per frame)

  floor     the same sequence with every noise term replaced by a constant. Whole-pixel trucks: zero (this also
            shows that the receiver bias, the near taps and the bisection have no camera term of their own). Half
            pixel truck: what the bilinear read of an aliased penumbra leaves.
  model 0   RenderSoftShadowNoiseModel 0, the legacy statements: the Poisson jitter is a view-space sawtooth that
            slides with the camera, the Vogel rotation a screen-pixel hash.
  model 1   the screen-pixel hash everywhere: every pixel a world point crosses re-rolls its kernel.
  model 2   the default: the projector surface kernels take no per-pixel term at all (the hash is replaced by its
            mean, 0.5: the Poisson disk is centred, the Vogel and search rotations are the slot's own, the bisection
            grid has one fixed shift).

Asserted (Poisson = the default kernel, and Vogel; metric and legacy penumbra):
  motion    positive control: the screen hash flickers (model 1, at least 3 steps per frame in the whole-pixel trucks);
            model 2 stays under 0.05 steps there (and so under model 0), and within 1 step of the floor in the
            half pixel truck.
  quality   against a 72-tap reference (model 1 averaged over six whole-pixel trucks) the low-frequency error of
            model 2 is at most half of model 0's on the Poisson kernel (the sawtooth displaces the edge), and no
            worse than the constant-noise render + 1 step on the Vogel kernel.
  still     model 2 does not read the screen hash at all (bit-identical with the hash replaced by another constant
            function of the pixel), wide penumbra and contact shadow.
  leak      a sun sample taken after a projector sample equals the sun sample taken alone (the still flag ends with
            its own sample); the fixture's sun kernel does read the screen hash (positive control).
  detail    a rig light on its own Detail receiver (RenderSpotShadowBiasModel 1) is the legacy kernel, bit-identical
            under every noise model. Reported: its flicker (the legacy sawtooth / hash). Stilling it too was built in
            round 1 and taken out: the two extra statements in the legacy function cost 0.18 ms per 16 lights at 4K
            for every projector, Detail or not (the driver's code shape, measured). RenderSpotShadowBiasModel 2 puts
            Detail lights on the surface kernel and so on model 2.
  identity  models 0 and 1 are byte-identical to BASE in every case.
  source    the pipeline uploads the setting clamped to 2 and settings.xml defaults it to 2; model 2 adds no shader
            input (no screen size, no derivative, no new uniform): it is reachable only through the surface kernel,
            whose switch (the bias model) is uploaded in the same stage as the noise model.
Reported, not asserted (attribution): which noise term of model 1 carries the flicker (kernel, metric search rotation,
bisection shift), on a penumbra the cap does not saturate.
Planted regressions must each be rejected: model 2 left on the screen hash, model 1 made still, the still flag
left set for the next sampler, the flag set outside the surface kernel's soft branch.
"""
import argparse
import json
import math
import re
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import rig_shadow_gl as R
import relight_spot_bias_gl_check as B
import relight_soft_shadow_quality_gl_check as Q
import relight_spot_metric_gl_check as M
from rig_shadow_presets_check import settings_defaults

BASE = '0494b1f4d19'   # main before the anchored noise model
RES = 320
HALF = .5              # half the width of plane the camera sees, metres: one pixel is 3.1 mm, about two map texels
EYE_X = 1.4            # the camera stands between the blocker and the plane and faces the plane squarely
EDGE = M.ball_edge(1.5) * math.sqrt(.5)   # the 45 degree point of the ball's shadow outline: the edge runs diagonally in the map


def view(scene, centre, half):
    """A camera square on the plane x = 0, looking at (0, centre): one pixel is exactly 2 * half / RES metres of the plane,
    so a truck by whole pixels moves every plane point by exactly that many pixels."""
    eye = np.array([EYE_X, centre[0], centre[1]])
    target = np.array([0., centre[0], centre[1]])
    cam = S.Camera(eye, target, RES, RES, math.degrees(2 * math.atan(half / EYE_X)))
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    return cam, g, d, n


def sequences(half):
    """name -> 6 camera centres (y, z) on the plane. Truck: sideways and up by a fixed pixel count per frame."""
    px = 2. * half / RES
    c0 = np.array([EDGE, 1.45 + EDGE])
    out = {}
    for name, step in (('truck 1 px', (1., 1.)), ('truck 3 px', (3., 2.)), ('truck 0.5 px', (.5, .5))):
        out[name] = [c0 + np.array(step) * px * k for k in range(6)]
    return out


def follow(frames, half):
    """frames: [(cam, g, v)]. The plane points under the pixels of frame 0, read in every frame (bilinear between the four
    pixels around the point: exact for a whole-pixel truck). Returns (values [K, N], keep [N], mask of the points in frame 0)."""
    cam0, g0, _ = frames[0]
    sel = g0['surf'] == 0
    p = g0['pos'][sel].astype(np.float64)
    hp = np.concatenate([p, np.ones((len(p), 1))], axis=1)
    keep = np.ones(len(p), bool)
    vals = []
    for cam, g, v in frames:
        clip = hp @ (cam.proj @ cam.view).T
        ndc = clip[:, :2] / clip[:, 3:4]
        x = (ndc[:, 0] * .5 + .5) * cam.W - .5
        y = (ndc[:, 1] * .5 + .5) * cam.H - .5
        i, j = np.floor(x + 1e-4).astype(int), np.floor(y + 1e-4).astype(int)
        fx, fy = np.clip(x - i, 0., 1.), np.clip(y - j, 0., 1.)
        inside = (i >= 0) & (i < cam.W - 1) & (j >= 0) & (j < cam.H - 1)
        i, j = np.clip(i, 0, cam.W - 2), np.clip(j, 0, cam.H - 2)
        on = g['surf'] == 0
        keep &= inside & on[j, i] & on[j, i + 1] & on[j + 1, i] & on[j + 1, i + 1]
        vals.append((v[j, i] * (1 - fx) + v[j, i + 1] * fx) * (1 - fy) + (v[j + 1, i] * (1 - fx) + v[j + 1, i + 1] * fx) * fy)
    return np.stack(vals), keep, sel


def flicker(vals, part):
    if part.sum() == 0:
        return None
    return float(np.abs(np.diff(vals[:, part], axis=0)).mean() * 255.)


def setup_case(r, host, defaults, case):
    """case: dict(radius, xc, cap, res, metric, vogel, detail). Returns (scene, lamp entry, run settings, metric vec4 or None)."""
    e = M.lamp(softness=0. if case['metric'] else case.get('softness', 8.), detail=case.get('detail', False))
    scene = M.scene_of([M.ball(case['xc'])])
    r.native_map(scene, B.light_of(e), e['fov'], case['res'])
    metric = M.metric_of(host, r, e, case['radius'])[1] if case['metric'] else None
    run = M.run_of(defaults, case['cap'], RenderSpotShadowDetailBias=True)
    return scene, e, run, metric


def frames_of(r, prog, views, e, run, metric, case, noise):
    model = B.MODEL_DEFAULT if case.get('detail') else B.MODEL_ALL   # a Detail light under the default keeps its own receiver
    return [(cam, g, M.render(r, prog, cam, d, n, e, run, metric, taps=16, vogel=case['vogel'], noise=noise, model=model))
            for cam, g, d, n in views]


def measure_case(r, host, defaults, case, programs, models=(0, 1, 2), seq_names=None):
    """programs: dict(live=, flat=). One row per sequence: flicker of the constant-noise render (floor) and of each model."""
    scene, e, run, metric = setup_case(r, host, defaults, case)
    half = case['half']
    rows = []
    for name, path in sequences(half).items():
        if seq_names and name not in seq_names:
            continue
        views = [view(scene, centre, half) for centre in path]
        flat_vals, keep, _ = follow(frames_of(r, programs['flat'], views, e, run, metric, case, 2), half)
        ref = flat_vals.mean(axis=0)
        part = keep & (ref > .05) & (ref < .95)
        row = dict(check='motion', case=case['name'], sequence=name, points=int(part.sum()), floor=flicker(flat_vals, part))
        for model in models:
            vals, k2, _ = follow(frames_of(r, programs['live'], views, e, run, metric, case, model), half)
            assert np.array_equal(k2, keep)
            row['model%d' % model] = flicker(vals, part)
        rows.append(row)
    return rows


def box(a, k=2):
    out = np.zeros_like(a)
    for dy in range(-k, k + 1):
        for dx in range(-k, k + 1):
            out += np.roll(np.roll(a, dy, 0), dx, 1)
    return out / (2 * k + 1) ** 2


def quality_case(r, host, defaults, case, programs):
    """Low-frequency (5 x 5 mean) and high-frequency RMS error of frame 0, in 8-bit steps, against a reference built from the
    screen hash averaged over six whole-pixel trucks (six independent kernels for every world point)."""
    scene, e, run, metric = setup_case(r, host, defaults, case)
    half = case['half']
    path = sequences(half)['truck 3 px']
    views = [view(scene, centre, half) for centre in path]
    vals, keep, sel = follow(frames_of(r, programs['base_live'], views, e, run, metric, case, 1), half)
    ref = np.full(sel.shape, np.nan)
    ref[sel] = np.where(keep, vals.mean(0), np.nan)
    ok = np.isfinite(ref)
    clean = np.nan_to_num(ref)
    m = ok & (clean > .05) & (clean < .95) & (box(ok.astype(float)) > .999)
    row = dict(check='quality', case=case['name'], pixels=int(m.sum()))
    for name, prog, noise in (('flat', programs['flat'], 2), ('model0', programs['live'], 0), ('model1', programs['live'], 1),
                              ('model2', programs['live'], 2)):
        v = frames_of(r, prog, views[:1], e, run, metric, case, noise)[0][2]
        err = np.where(ok, v - clean, 0.)
        low = box(err)
        row[name + '_band'] = float(np.sqrt((low[m] ** 2).mean()) * 255.)
        row[name + '_grain'] = float(np.sqrt(((err - low)[m] ** 2).mean()) * 255.)
    return row


CASES = [
    dict(name='poisson metric, cap 8 (the reported settings)', radius=.10, xc=1.5, cap=8., res=2048, metric=True, vogel=False, half=HALF),
    dict(name='poisson metric, wide', radius=.04, xc=1.5, cap=24., res=2048, metric=True, vogel=False, half=HALF),
    dict(name='poisson metric, under the cap', radius=.012, xc=1.5, cap=24., res=2048, metric=True, vogel=False, half=HALF),
    dict(name='vogel metric, wide', radius=.04, xc=1.5, cap=24., res=2048, metric=True, vogel=True, half=HALF),
    dict(name='vogel metric, under the cap', radius=.012, xc=1.5, cap=24., res=2048, metric=True, vogel=True, half=HALF),
    dict(name='poisson legacy penumbra', radius=.04, xc=2.2, cap=12., res=2048, metric=False, vogel=False, half=HALF),
    dict(name='vogel legacy penumbra', radius=.04, xc=2.2, cap=12., res=2048, metric=False, vogel=True, half=HALF),
    dict(name='poisson, rig Detail light', radius=.04, xc=2.2, cap=12., res=2048, metric=False, vogel=False, half=HALF, detail=True),
    dict(name='vogel, rig Detail light', radius=.04, xc=2.2, cap=12., res=2048, metric=False, vogel=True, half=HALF, detail=True),
]
QUICK = (0, 3, 7)   # the cases a planted regression is run against
WHOLE = ('truck 1 px', 'truck 3 px')
LIMIT = dict(control=3., whole=.05, over_model0=.05, over_floor=1., band_share=.5, band_over_flat=1., step_over_floor=3.)

# Exact statements of shadowUtil.glsl (a planted regression or a probe replaces one; drift fails the run).
SCREEN_HASH = '    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n'
STILL_IGN = '    if (g_spot_still) return 0.5;\n'
STILL_SET = '        g_spot_still = soft_shadow_noise_model == 2;\n'
STILL_CLEAR = '        g_spot_still = false;   // [SoftShadow still] ends with its own sample\n'
LEGACY_JIT = '            float jit = fract(pos_screen.y * 0.666666666);\n'
LEGACY_PHI = ('            float phi = 6.2831853 * fract(52.9829189 *\n'
              '                        fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n')
# every noise term at a constant: the screen hash, the anchored value, the Detail light's legacy statements
FLAT = [(SCREEN_HASH, '    return 0.37;\n'),
        (LEGACY_JIT, '            float jit = 0.5;\n'), (LEGACY_PHI, '            float phi = 0.0;\n')]
SHIFT = '        float shift = hi * SPOT_METRIC_BISECT_STEP * softShadowIGN();\n'
KERNEL_PHI = '        float phi = softShadowPhi(pr, slot_phase);\n'
KERNEL_JIT = 'all:softShadowJitter(fract(pos_screen.y * 0.666666666))'
NO_SEARCH = (M.SEARCH_PHI, '(g_spot_slot * SOFT_NOISE_SLOT_PHASE + SPOT_METRIC_SEARCH_PHASE)')
NO_SHIFT = (SHIFT, '        float shift = hi * SPOT_METRIC_BISECT_STEP * 0.37;\n')
NO_KERNEL = [(KERNEL_PHI, '        float phi = slot_phase;\n'), (KERNEL_JIT, '0.5')]


def negatives():
    return {
        'model2_left_on_the_screen_hash': [(STILL_IGN, '')],
        'model2_never_selected': [(STILL_SET, STILL_SET.replace('== 2', '== 3'))],
        'model1_made_still': [(STILL_SET, STILL_SET.replace('== 2', '!= 0'))],
        'still_flag_left_set': [(STILL_CLEAR, '')],
        'still_flag_set_for_every_sampler': [('bool g_spot_still = false;', 'bool g_spot_still = true;')],
    }


def motion_failures(row, case):
    f = []
    tag = '%s, %s: ' % (row['case'], row['sequence'])
    if row['points'] < 800:
        return [tag + 'too few penumbra points (%d)' % row['points']]
    if case.get('detail'):
        # the legacy Detail kernel: not stilled (see the module text); it must simply be the same under every model
        return [] if row['model0'] == row['model1'] == row['model2'] else [tag + 'the Detail kernel changes with the noise model']
    if row['sequence'] in WHOLE:
        if row['floor'] > .05:
            f.append(tag + 'the constant-noise render changes with the camera (%.3f): the fixture, the bias or the search has a camera term' % row['floor'])
        if not case.get('detail') and row['model1'] < LIMIT['control']:
            f.append(tag + 'positive control failed: the screen hash does not flicker here (%.2f)' % row['model1'])
        if row['model2'] > LIMIT['whole']:
            f.append(tag + 'model 2 flickers with the camera (%.2f steps per frame)' % row['model2'])
        if row['model2'] > row['model0'] + LIMIT['over_model0']:
            f.append(tag + 'model 2 flickers more than model 0 (%.2f against %.2f)' % (row['model2'], row['model0']))
    elif row['model2'] > row['floor'] + LIMIT['over_floor']:
        # Judged against the floor only: between pixels the bilinear read of a 12-tap penumbra is itself worth several steps,
        # and by how much depends on where the legacy sawtooth happens to put the kernel (model 0 lands 1 step either side).
        f.append(tag + 'model 2 flickers under a half-pixel move (%.2f, floor %.2f)' % (row['model2'], row['floor']))
    return f


def quality_failures(row, case):
    f = []
    if row['pixels'] < 800:
        return ['%s: too few penumbra pixels for the quality rows' % row['case']]
    if not case['vogel'] and row['model2_band'] > LIMIT['band_share'] * row['model0_band']:
        f.append('%s: the Poisson kernel of model 2 is as far off the reference as the sawtooth (%.1f against %.1f)' %
                 (row['case'], row['model2_band'], row['model0_band']))
    if case['vogel'] and row['model2_band'] > row['flat_band'] + LIMIT['band_over_flat']:
        f.append('%s: the anchored rotation bands more than no rotation (%.1f against %.1f)' % (row['case'], row['model2_band'], row['flat_band']))
    return f


def noise_check(r, host, defaults, edits=()):
    """Model 2 reads no per-pixel hash: the render is bit-identical when the screen hash is another function of the pixel."""
    rows, failures = [], []
    live = B.link(r, M.Mut(list(edits)), 'still live')
    other = B.link(r, M.Mut(list(edits) + [(SCREEN_HASH, '    return fract(dot(gl_FragCoord.xy, vec2(0.37, 0.61)));\n')]), 'still, other hash')
    for case in (CASES[1], CASES[3]):
        scene, e, run, metric = setup_case(r, host, defaults, case)
        views = [view(scene, sequences(HALF)['truck 1 px'][0], HALF)]
        row = dict(check='still', case=case['name'])
        for model in (1, 2):
            a, b = (frames_of(r, prog, views, e, run, metric, case, model)[0][2] for prog in (live, other))
            part = (views[0][1]['surf'] == 0) & (a > .02) & (a < .98)
            row['model%d_depends_on_hash' % model] = float((np.abs(a - b)[part] > 1e-6).mean()) if part.any() else None
            row['pixels'] = int(part.sum())
        rows.append(row)
        if row['pixels'] < 2000 or row['model1_depends_on_hash'] < .5:
            failures.append('still %s: positive control failed, model 1 does not read the hash here (%s)' % (case['name'], json.dumps(row)))
        if row['model2_depends_on_hash'] != 0.:
            failures.append('still %s: model 2 still reads the screen hash on %.3f of the penumbra' % (case['name'], row['model2_depends_on_hash']))
    e = M.lamp()
    scene = M.scene_of([M.card(.04)])
    cam, d, n, coord, mask = M.edge_view(scene, 0., .02)
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    _, metric = M.metric_of(host, r, e, .04)
    run = M.run_of(defaults, 24.)
    a, b = (M.render(r, prog, cam, d, n, e, run, metric, taps=16, vogel=True, noise=2) for prog in (live, other))
    row = dict(check='still', case='contact', identical=bool(np.array_equal(a, b)), pixels=int((mask & (a > .02) & (a < .98)).sum()))
    rows.append(row)
    if not row['identical'] or row['pixels'] < 100:
        failures.append('still: a contact shadow of model 2 reads the screen hash (%s)' % json.dumps(row))
    return rows, failures


LEAK_SOURCE = """
out vec4 frag_color;
uniform int probe_first;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz;vec3 n=getNorm(tc).xyz;
float spot=probe_first!=0?sampleSpotShadow(p,n,0,tc):1.;
frag_color=vec4(sampleDirectionalShadow(p,n,tc),spot,0.,1.);}
"""


def leak_check(r, host, defaults, edits=()):
    """A sun sample after a projector sample of model 2 equals the sun sample alone."""
    rows, failures = [], []
    case = CASES[3]
    scene, e, run, metric = setup_case(r, host, defaults, case)
    cam, g, d, n = view(scene, sequences(HALF)['truck 1 px'][0], HALF)

    def render(prog, first):
        r.target_float(cam)
        r._begin(prog)
        r.inputs(prog, cam, d, n)
        r.native_uniforms(prog, cam, e, run)
        Q.sun_state(r, prog, cam, e, defaults, True, noise=2, retire=1)
        r.uniform(prog, 'soft_shadow_max', case['cap'])
        r.gl.ui(r.gl.loc(prog, b'spot_shadow_bias_model'), B.MODEL_ALL)
        r.gl.ui(r.gl.loc(prog, b'spot_shadow_fade_model'), 1)
        r.uniform(prog, 'spot_shadow_metric[0]', *metric)
        r.gl.ui(r.gl.loc(prog, b'probe_first'), int(first))
        return r.read_float(cam)
    live = Q.link_src(r, M.Mut(list(edits)), LEAK_SOURCE, 'leak', defines=Q.SUN_DEFINES)
    flat = Q.link_src(r, M.Mut(list(edits) + [(SCREEN_HASH, '    return 0.37;\n')]), LEAK_SOURCE, 'leak constant hash', defines=Q.SUN_DEFINES)
    alone, after = render(live, 0), render(live, 1)
    sun = alone[..., 0]
    part = (g['surf'] == 0) & (sun > .02) & (sun < .98)
    hashed = float((np.abs(sun - render(flat, 0)[..., 0])[part] > 1e-6).mean()) if part.any() else 0.
    spot = after[..., 1]
    row = dict(check='leak', sun_penumbra_pixels=int(part.sum()), sun_reads_screen_hash=hashed,
               projector_penumbra_pixels=int(((g['surf'] == 0) & (spot > .02) & (spot < .98)).sum()),
               sun_identical_after_projector=bool(np.array_equal(sun, after[..., 0])))
    rows.append(row)
    if part.sum() < 2000 or hashed < .5 or row['projector_penumbra_pixels'] < 2000:
        failures.append('leak: the fixture does not exercise both kernels (%s)' % json.dumps(row))
    if not row['sun_identical_after_projector']:
        failures.append('leak: a sun sample after a projector sample differs from the sun sample alone')
    return rows, failures


def identity_check(r, host, defaults, live, base):
    """Models 0 and 1 against BASE, every case, frame 0."""
    checks = {}
    for case in CASES:
        scene, e, run, metric = setup_case(r, host, defaults, case)
        views = [view(scene, sequences(HALF)['truck 1 px'][0], HALF)]
        for model in (0, 1):
            a = frames_of(r, live, views, e, run, metric, case, model)[0][2]
            b = frames_of(r, base, views, e, run, metric, case, model)[0][2]
            checks['%s, model %d' % (case['name'], model)] = bool(np.array_equal(a, b))
    return checks


def body_of(text, signature):
    start = text.index(signature)
    depth, i = 0, text.index('{', start)
    while True:
        depth += text[i] == '{'
        depth -= text[i] == '}'
        if depth == 0:
            return text[start:i + 1]
        i += 1


def reach_contracts(pipeline, shader, base_shader):
    """Review round 1 (GLM a): what model 2 needs, and where it can run.

    Model 2 adds no shader input: no screen size, no inverse projection term, no derivative, no new uniform (counted
    against BASE below). Its one flag is set only in the soft branch of pcfSpotShadowSurface, which is reachable only with
    g_spot_bias_mode == 1, i.e. spot_shadow_bias_model != 0. A program that never receives that upload keeps the shader
    default 0 (VCam / Prism feeds, probe captures, the actor key pass: whatever they bind). The bias model is uploaded in ONE
    place, the Tuning stage of LLPipeline::bindSunShadowState, and the noise model in that same stage: "can reach the still
    kernel" implies "has this frame's noise model". Every call of bindSunShadowState is inside bindDeferredShader or directly
    follows a bindDeferredShader of the same program."""
    sun = body_of(pipeline, 'void LLPipeline::bindSunShadowState(')
    tuning = sun[sun.index('stage == SunShadowStage::Tuning)'):sun.index('stage == SunShadowStage::Directions)')]
    deferred = body_of(pipeline, 'void LLPipeline::bindDeferredShader(LLGLSLShader& shader, LLRenderTarget* light_target')
    calls = [i for i in range(len(pipeline)) if pipeline.startswith('bindSunShadowState(shader', i) and not pipeline.startswith('void LLPipeline::', i - 17)]
    start = pipeline.index('void LLPipeline::bindDeferredShader(LLGLSLShader& shader, LLRenderTarget* light_target')
    inside = [start <= i < start + len(deferred) for i in calls]
    after = ['bindDeferredShader(shader);' in pipeline[max(i - 120, 0):i] for i in calls]
    surface = body_of(shader, 'float pcfSpotShadowSurface(')
    soft = surface[surface.index('if (soft_shadow_enable != 0 || projector_softness > 0.0)'):]
    count = lambda text, token: len(re.findall(token, text))
    return {
        'bias_and_noise_model_uploaded_in_the_same_stage': tuning.count('LLStaticHashedString("spot_shadow_bias_model")') == 1 and
            tuning.count('LLStaticHashedString("soft_shadow_noise_model")') == 1 and
            pipeline.count('"spot_shadow_bias_model"') == 1 and pipeline.count('"soft_shadow_noise_model"') == 1,
        'every_sun_state_bind_follows_a_deferred_bind': len(calls) == 5 and all(a or b for a, b in zip(inside, after)) and sum(inside) == 4,
        'bias_model_default_is_legacy': 'uniform int spot_shadow_bias_model = 0;' in shader,
        'still_flag_set_once_in_the_surface_soft_branch': shader.count('g_spot_still = soft_shadow_noise_model == 2;') == 1 and
            soft.count(STILL_SET) == 1 and shader.count('g_spot_still = ') == 3 and shader.count(STILL_CLEAR) == 1 and
            'bool g_spot_still = false;' in shader and shader.count('pcfSpotShadowSurface(') == 2,
        'still_flag_read_once': shader.count('if (g_spot_still)') == 1 and STILL_IGN in body_of(shader, 'float softShadowIGN()'),
        'no_new_shader_inputs': all(count(shader, token) == count(base_shader, token) for token in
                                    (r'gl_FragCoord', r'screen_res', r'inv_proj', r'dFd[xy]', r'fwidth')) and
            sorted(re.findall(r'^uniform [^;]+;', shader, re.M)) == sorted(re.findall(r'^uniform [^;]+;', base_shader, re.M) +
                                                                          ['uniform float soft_shadow_min_radius = 0.0;']),
    }


def source_contracts(tree):
    pipeline = tree.read('indra/newview/pipeline.cpp')
    settings = tree.read('indra/newview/app_settings/settings.xml')
    block = settings.split('<key>RenderSoftShadowNoiseModel</key>')[1].split('</map>')[0]
    shader = tree.read(B.SHADER)
    return {
        **{'reach_' + k: v for k, v in reach_contracts(pipeline, shader, G.SourceTree(BASE).read(B.SHADER)).items()},
        'upload_clamps_to_2': pipeline.count('shader.uniform1i(LLStaticHashedString("soft_shadow_noise_model"), (S32)llmin((U32)soft_noise_model, 2U));') == 1,
        'setting_default_2': '<integer>2</integer>' in block and '<string>U32</string>' in block,
        'cached_default_2': pipeline.count('static LLCachedControl<U32> soft_noise_model(gSavedSettings, "RenderSoftShadowNoiseModel", 2);') == 1,
        'shader_default_legacy': 'uniform int soft_shadow_noise_model = 0;' in shader,
    }


def attribution(r, host, defaults):
    """Which noise term of the screen hash carries the flicker, where the cap does not hide the search (reported)."""
    rows = []
    flat = B.link(r, M.Mut(FLAT), 'constant noise')
    variants = {'all terms': [], 'kernel only': [NO_SEARCH, NO_SHIFT], 'search rotation only': [NO_SHIFT] + NO_KERNEL,
                'bisection shift only': [NO_SEARCH] + NO_KERNEL}
    for case in (CASES[2], CASES[4]):
        for label, edits in variants.items():
            prog = B.link(r, M.Mut(edits), 'attribution ' + label)
            for row in measure_case(r, host, defaults, case, dict(live=prog, flat=flat), models=(0, 1), seq_names=WHOLE):
                rows.append(dict(check='attribution', case=case['name'], sequence=row['sequence'], term=label,
                                 model0=row['model0'], model1=row['model1']))
    return rows


def run_checks(r, host, defaults, edits=(), cases=None):
    """(rows, failures, program) of the motion, quality, noise and leak checks on the worktree shader with `edits`."""
    edits = list(edits)
    live = B.link(r, M.Mut(edits), 'motion live')
    flat = B.link(r, M.Mut(edits + FLAT), 'motion constant noise')
    programs = dict(live=live, flat=flat, base_live=r.visibility)
    rows, failures = [], []
    for i, case in enumerate(CASES):
        if cases is not None and i not in cases:
            continue
        for row in measure_case(r, host, defaults, case, programs):
            rows.append(row)
            failures += motion_failures(row, case)
        if not case.get('detail'):
            row = quality_case(r, host, defaults, case, programs)
            rows.append(row)
            failures += quality_failures(row, case)
    for fn in (noise_check, leak_check):
        more, f = fn(r, host, defaults, edits)
        rows += more
        failures += f
    return rows, failures, live


def evaluate(out, do_negatives=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    defaults = settings_defaults()
    r = R.Runner('WORKTREE')
    print('[soft-motion] GPU', r.gl.renderer, flush=True)
    host = M.HeaderHost(out, r.tree.read(M.HEADER))
    failures, result = [], {}
    contracts = source_contracts(r.tree)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    rows, f, live = run_checks(r, host, defaults)
    failures += f
    for row in rows:
        print('[soft-motion]', json.dumps(row), flush=True)
    base = B.link(r, G.SourceTree(BASE), 'BASE visibility')
    identity = identity_check(r, host, defaults, r.visibility, base)
    failures += ['identity ' + k for k, ok in identity.items() if not ok]
    extra = attribution(r, host, defaults)
    for row in extra:
        print('[soft-motion]', json.dumps(row), flush=True)
    result.update(contracts=contracts, rows=rows, identity=identity, attribution=extra)
    if do_negatives:
        rejected = {}
        for name, edits in negatives().items():
            try:
                _, f, mutant = run_checks(r, host, defaults, edits, cases=QUICK)
                why = f[0] if f else 'identity'
                caught = bool(f) or not all(identity_check(r, host, defaults, mutant, base).values())
                if not caught:   # source-level regressions: the contracts on the mutated shader text
                    caught = not all(source_contracts(M.Mut(edits)).values())
                    why = 'source contract'
            except AssertionError as ex:
                caught, why = True, 'raised ' + str(ex)[:100]
            rejected[name] = caught
            print('[soft-motion]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, '|', why[:160], flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, default=str) + '\n', encoding='utf-8')
    print('[soft-motion]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_soft_shadow_motion')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
