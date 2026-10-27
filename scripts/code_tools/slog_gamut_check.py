"""Log gamut numerics/contracts; --gpu runs production GLSL (no viewer build).

Run with PYTHONUTF8=1. GPU work refuses to start while AlchemyTest.exe runs.
Sources: Sony Technical Summary v1.0 pp.7-8 and ARRI LogC VFX 2017 p.10:
https://www.sony.jp/ls-camera/knowledge/pdf/TechnicalSummary_for_S-Gamut3Cine_S-Gamut3_S-Log3_V1_00.pdf
https://www.arri.com/resource/blob/31918/66f56e6abb6e5b6553929edf9aa7483e/2017-03-alexa-logc-curve-in-vfx-data.pdf
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import numpy as np

# Lead-approved bound for the NEW delivered-output test only (PURITY_BRIEF item 6a).
# Absolute decoded-linear error uses the requested half-10-bit-code number;
# other existing tests keep their original tolerances. HEAD is measured separately.
DELIVERED_TOL = 0.5 / 1023.0
LUMA = np.array([.2126, .7152, .0722])
ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / 'indra/newview/app_settings/shaders'
TONE = 'class1/deferred/tonemapUtilF.glsl'
ROUND1 = ROOT/'scripts/code_tools/slog_gamut_fixtures/round1'
PRIMARIES = (
    ((.64, .33), (.30, .60), (.15, .06)),
    ((.730, .280), (.140, .855), (.100, -.050)),
    ((.766, .275), (.225, .800), (.089, -.087)),
    ((.684, .313), (.221, .848), (.0861, -.1020)),
)


def rgb_xyz(primaries):
    p = np.array([(x/y, 1., (1-x-y)/y) for x, y in primaries]).T
    white = np.array([.3127/.3290, 1., (1-.3127-.3290)/.3290])
    return p @ np.diag(np.linalg.solve(p, white))


def matrices():
    source = (SHADERS/TONE).read_text(encoding='utf-8')
    result = [np.eye(3)]
    for name in ('SG3', 'CINE', 'AWG3'):
        match = re.search(r'const mat3 SLF_'+name+r' = transpose\(mat3\((.*?)\)\);', source, re.S)
        assert match, 'missing production gamut matrix '+name
        result.append(np.array([float(x) for x in match[1].split(',')]).reshape(3, 3))
    return result


def slog3_inverse(code):
    return np.where(code >= 171.2102946929/1023.,
                    10.**((code*1023.-420.)/261.5)*.19-.01,
                    (code*1023.-95.)*.01125/(171.2102946929-95.))


def apply_purity(values, strength):
    if strength == 1.:
        return values.copy()
    y = values @ LUMA
    return y[..., None] + strength*(values-y[..., None])


def inverse_purity(values, strength):
    if strength == 1.:
        return values.copy()
    y = values @ LUMA
    return y[..., None] + (values-y[..., None])/strength


def Round1Runner():
    import sensor_post_gl_check as P
    class Runner(P.Runner):
        def source(self, path, ref=None):
            if ref == 'ROUND1':
                saved = ROUND1/path
                if path in (TONE, 'class1/alchemy/colorCorrectF.glsl'):
                    assert saved.is_file(), 'round-1 shader oracle missing: '+str(saved)
                    return saved.read_text(encoding='utf-8')
                return super().source(path, ref='HEAD')
            return super().source(path, ref=ref)
    return Runner()


def gpu_idle():
    running = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command',
                              '(Get-Process -Name AlchemyTest -ErrorAction SilentlyContinue | Measure-Object).Count'],
                             capture_output=True, text=True, timeout=15)
    return running.returncode == 0 and running.stdout.strip() == '0'


def cpu():
    purity_contracts()
    mats = matrices()
    patches = np.array([[0., 0., 0.], [.18]*3, [1.]*3, [1., 0., 0.],
                        [0., 1., 0.], [0., 0., 1.], [4., .02, 2.]])
    for i, matrix in enumerate(mats):
        derived = np.linalg.solve(rgb_xyz(PRIMARIES[i]), rgb_xyz(PRIMARIES[0]))
        np.testing.assert_allclose(matrix, derived, atol=1e-6, rtol=0)
        np.testing.assert_allclose(matrix.sum(axis=1), 1., atol=2e-8, rtol=0)
        np.testing.assert_allclose(matrix @ np.ones(3), np.ones(3), atol=2e-8, rtol=0)
        np.testing.assert_allclose(patches @ matrix.T @ np.linalg.inv(matrix).T,
                                   patches, atol=2e-14, rtol=0)
    # Published Sony equations; the independent primary-derived inverse must
    # recover a pure Rec.709 red from the production S-Gamut3 coefficients.
    red = mats[1] @ np.array([1., 0., 0.])
    code = (420. + np.log10((red+.01)/.19)*261.5)/1023.
    independent = np.linalg.solve(rgb_xyz(PRIMARIES[1]), rgb_xyz(PRIMARIES[0]))
    np.testing.assert_allclose(np.linalg.solve(independent, slog3_inverse(code)), [1., 0., 0.], atol=1e-8, rtol=0)
    print('PASS: primaries-derived matrices (1e-6), row sums, white, inverse round trips')
    contracts()


def purity_contracts():
    provenance = json.loads((ROUND1/'provenance.json').read_text(encoding='utf-8'))
    for name, digest in provenance['sha256'].items():
        assert hashlib.sha256((ROUND1/name).read_bytes()).hexdigest() == digest, 'round-1 fixture drift'
    tone = (SHADERS/TONE).read_text(encoding='utf-8')
    assert 'uniform float tonemap_slog_purity;' in tone, 'missing production purity uniform'
    encode = tone.split('vec3 encode_slog_family(vec3 c)', 1)[1].split('// Invert delivered', 1)[0]
    assert encode.index('c *= exp2(ev)') < encode.index('slf_apply_purity(c)') < encode.index('slf_gamut_matrix() * c')
    assert 'if (purity < 1.0' in tone, 'purity 1 must skip new arithmetic'
    decode = tone.split('vec3 decode_slog_family(vec3 code)', 1)[1].split('vec3 applyExposure', 1)[0]
    assert decode.index('inverse(slf_gamut_matrix()) * c') < decode.index('slf_inverse_purity(c)') < decode.index('return c * exp2')
    old = subprocess.check_output(['git', 'show', 'HEAD:indra/newview/app_settings/shaders/'+TONE],
                                  cwd=ROOT, text=True, encoding='utf-8', timeout=15)
    assert tone.split('vec3 applyToneMap(vec3 color)', 1)[1] == old.split('vec3 applyToneMap(vec3 color)', 1)[1], 'operators 0..7 must not change'
    luma = np.array([.2126, .7152, .0722])
    patches = np.array([[0., 0., 0.], [.18]*3, [1.]*3, [1., 0., 0.], [1., 0., 1.], [0., 1., 0.], [35., .04, .2]])
    for strength in (.6, .9, 1.):
        y = patches @ luma
        reduced = y[:, None] + strength*(patches-y[:, None])
        np.testing.assert_array_equal(reduced[:3], patches[:3])
        np.testing.assert_allclose(reduced @ luma, y, atol=2e-15, rtol=0)
        assert np.all(reduced >= 0.)
        inverse_y = reduced @ luma
        recovered = inverse_y[:, None] + (reduced-inverse_y[:, None])/strength
        np.testing.assert_allclose(recovered, patches, atol=1e-14, rtol=0)
        if strength == .9:
            np.testing.assert_allclose(reduced[3], [.92126, .02126, .02126], atol=1e-15, rtol=0)
    print('PASS: purity formula, exact neutrals/white, luminance, nonnegativity and inverse')


def contracts():
    tone = (SHADERS/TONE).read_text(encoding='utf-8')
    encode = tone.split('vec3 encode_slog_family(vec3 c)', 1)[1].split('vec3 applyExposure', 1)[0]
    assert encode.index('c *= exp2(ev)') < encode.index('slf_gamut_matrix() * c') < encode.index('c += vec3(0.18 * toe)')
    assert 'if (tonemap_slog_gamut != 0)' in encode, 'legacy must skip matrix arithmetic'
    old = subprocess.check_output(['git', 'show', 'HEAD:indra/newview/app_settings/shaders/'+TONE],
                                  cwd=ROOT, text=True, encoding='utf-8', timeout=15)
    old_encode = old.split('vec3 encode_slog_family(vec3 c)', 1)[1].split('vec3 applyExposure', 1)[0]
    current_body = encode.split('// Invert delivered log CODES', 1)[0]
    current_body = re.sub(r'    c = slf_apply_purity\(c\);[^\n]*\n', '', current_body)
    current_body = re.sub(r'    if \(tonemap_slog_gamut != 0\).*?\n        c = max\(slf_gamut_matrix\(\) \* c, vec3\(0.0\)\);\n', '', current_body, flags=re.S)
    assert current_body.rstrip() == old_encode.rstrip(), 'legacy encoder arithmetic drift'
    cpp = (ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    assert cpp.count('uploadSLogCaptureUniforms(shader);') == 1
    assert cpp.count('uploadSLogCaptureUniforms(*shader);') == 1
    assert 'llclamp(gamut(), 0, 3)' in cpp
    control = (ROOT/'indra/newview/llviewercontrol.cpp').read_text(encoding='utf-8')
    assert 'AlchemyToneMapSLogPurity' not in control
    assert 'llclamp(purity(), 0.6f, 1.f)' in cpp
    assert 'AlchemyToneMapSLogGamut' not in control, 'uniform-only setting must not reload shaders'
    color = (SHADERS/'class1/alchemy/colorCorrectF.glsl').read_text(encoding='utf-8')
    assert 'decode_slog_family(desired)' in color, 'godray reconstruction must undo the gamut'
    assert 'decode_slog_family(cameraTransfer(scene.rgb))' in color, 'retain energy beyond clipped camera codes'
    assert 'inverse(slf_gamut_matrix()) * c' in tone
    # Fail closed if another executable encoder/inverse consumer appears.
    consumers = {p.relative_to(SHADERS).as_posix() for p in SHADERS.rglob('*.glsl')
                 if re.search(r'\bapplyToneMap\s*\(', p.read_text(encoding='utf-8'))}
    assert consumers == {TONE, 'class1/alchemy/colorCorrectF.glsl',
                         'class1/deferred/postDeferredTonemap.glsl',
                         'class1/deferred/prismPreviewF.glsl', 'class1/deferred/prismProjectorFeedF.glsl'}, consumers
    h = (ROOT/'indra/llrender/llshadermgr.h').read_text(encoding='utf-8')
    enum = h.split('typedef enum', 1)[1].split('END_RESERVED_UNIFORMS', 1)[0]
    enum = re.sub(r'//[^\n]*|/\*.*?\*/', '', enum, flags=re.S)
    names = re.findall(r'\b([A-Z][A-Z0-9_]*)\s*(?:=\s*0)?\s*,', enum)
    reserved = re.findall(r'mReservedUniforms.push_back\("([^\"]+)"\)',
                          (ROOT/'indra/llrender/llshadermgr.cpp').read_text(encoding='utf-8'))
    assert len(names) == len(reserved)
    assert names.index('TONEMAP_SLOG_GAMUT') == reserved.index('tonemap_slog_gamut')
    assert names.index('TONEMAP_SLOG_PURITY') == reserved.index('tonemap_slog_purity')
    from xml.etree import ElementTree as ET
    setting = ET.parse(ROOT/'indra/newview/app_settings/settings_alchemy.xml').getroot().find('map')
    children = list(setting)
    value = children[next(i for i, n in enumerate(children) if n.text == 'AlchemyToneMapSLogGamut')+1]
    assert value.find('integer').text == '1'  # Persist
    assert list(value)[-1].text == '1'  # Value
    purity_setting = children[next(i for i, n in enumerate(children) if n.text == 'AlchemyToneMapSLogPurity')+1]
    assert purity_setting.find('integer').text == '1'
    assert purity_setting.find('real').text == '0.9'
    assert 'F32' in [n.text for n in purity_setting.findall('string')]
    for file in ('floater_lightbox_settings.xml', 'panel_dpage_look_image.xml'):
        tree = ET.parse(ROOT/'indra/newview/skins/default/xui/en'/file)
        combo = next(n for n in tree.iter('combo_box') if n.get('control_name') == 'AlchemyToneMapSLogGamut')
        assert [n.get('value') for n in combo.findall('combo_box.item')] == ['0', '1', '2', '3']
        slider = next(n for n in tree.iter('slider_bar' if file.startswith('floater') else 'slider')
                      if n.get('control_name') == 'AlchemyToneMapSLogPurity')
        assert (float(slider.get('min_val')), float(slider.get('max_val')), float(slider.get('increment'))) == (.6, 1., .01)
        assert slider.get('tool_tip') == 'How pure SL colours may get in log footage, like a real camera sensor. 0.90 = camera-like; 1.00 = original. Greys and brightness are unchanged.'
        saturation = next(n for n in tree.iter('slider_bar' if file.startswith('floater') else 'slider')
                          if n.get('control_name') == 'AlchemyToneMapSaturation')
        assert 'use Sensor purity' in saturation.get('tool_tip')
    assert 'if (saturation) v->setEnabled(!slog)' in (ROOT/'indra/newview/alfloaterlightbox.cpp').read_text(encoding='utf-8')
    assert 'view->setEnabled(gSavedSettings.getS32("AlchemyRenderTonemapType") != 8)' in (ROOT/'indra/newview/llfloaterdirector.cpp').read_text(encoding='utf-8')
    assert 'findRowBySetting("AlchemyToneMapSaturation")' in (ROOT/'indra/newview/llfloaterdirector.cpp').read_text(encoding='utf-8')
    synonyms = json.loads((ROOT/'scripts/code_tools/director_kit/synonyms.json').read_text(encoding='utf-8'))
    aliases = next(g for g in synonyms['groups'] if 's-gamut3' in g)
    assert set(('gamut', 'sgamut', 's-gamut3.cine', 'awg3', 'arri wide gamut', 'color space',
                'colour space', 'magic bullet', 'aces idt', 'oversaturated', 'too saturated')) <= set(aliases)
    purity_aliases = next(g for g in synonyms['groups'] if 'sensor purity' in g)
    assert set(('purity', 'saturation', 'desaturate', 'crosstalk', 'neon', 'too saturated', 'oversaturated', 'chroma')) <= set(purity_aliases)
    print('PASS: encoder order, inverse consumer, both uniform uploads, persistence/UI, no reload')


def gpu():
    # Importing the harness creates no context; Runner does, after the process guard.
    import ctypes as C
    import sensor_post_gl_check as P
    r = Round1Runner()
    failures = []
    measurements = []
    def close(label, actual, expected, **tolerance):
        try:
            np.testing.assert_allclose(actual, expected, **tolerance)
        except AssertionError as error:
            failures.append(label + ': ' + str(error))
            measurements.append(dict(test=label, max_absolute_error=float(np.max(np.abs(actual-expected)))))
    def group(label, start):
        print(('PASS GPU: ' if len(failures) == start else 'FAIL GPU: ') + label, flush=True)
    mats = matrices()
    patches = np.array([[0., 0., 0.], [.001]*3, [.01125]*3, [.18]*3, [1.]*3,
                        [1., 0., 0.], [0., 1., 0.], [0., 0., 1.],
                        [1., 1., 0.], [1., 0., 1.], [0., 1., 1.], [4., .02, 2.]], np.float32)[None]
    w = patches.shape[1]
    tex = r.upload('patches', patches, fmt=0x8814)
    black = r.upload('black', np.zeros_like(patches), fmt=0x8814)
    exp = r.upload('exp', np.ones((1, 1, 3), np.float32), fmt=0x8814)
    dst = r.target('codes', w, 1, fmt=0x8814)
    textures = [('diffuseRect', tex), ('diffuseMap', black), ('bloomMap', black), ('exposureMap', exp)]
    base = dict(tonemap_slog_purity=1., tonemap_type=8, tonemap_grade=(1., 1.), exposure=1., bloom_strength=0.)
    # Probe the actual production linear operator before any OETF precision.
    purity_program = r.gl.program()
    for module in P.common_modules()+('class1/deferred/tonemapUtilF.glsl', 'class1/deferred/LPMUtil.glsl'):
        r.gl.attach(purity_program, r.compile(r.source(module)))
    r.gl.attach(purity_program, r.compile('''
in vec2 vary_fragcoord;
uniform sampler2D diffuseRect;
out vec4 frag_color;
vec3 slf_apply_purity(vec3 c);
void main() { frag_color=vec4(slf_apply_purity(texture(diffuseRect,vary_fragcoord).rgb),1.0); }
'''))
    r.gl.attach(purity_program, r.base.vertex)
    r.gl.link(purity_program)
    ok = C.c_int()
    r.gl.getprog(purity_program, 0x8B82, C.byref(ok))
    assert ok.value, 'production purity operator link'
    for strength in (.6, .9, 1.):
        pure = r.draw(purity_program, dst, w, 1, [('diffuseRect', tex)],
                      dict(tonemap_slog_purity=strength))[0][..., :3]
        assert pure[:, :5].tobytes() == patches[:, :5].tobytes(), 'production neutral/white exactness'
        assert np.all(pure >= 0.)
        # Forward-error allowance for the float32 dot, subtraction, multiply,
        # add and luma check, bounded by eight eps times the largest input.
        arithmetic_error = 8*np.finfo(np.float32).eps*float(np.max(patches))
        close(f'linear operator luminance purity={strength}', pure @ LUMA,
              patches @ LUMA, atol=arithmetic_error, rtol=0)
        if strength == .9:
            close('production pure red at .9', pure[0, 5], np.array([.92126, .02126, .02126]), atol=1e-7, rtol=0)
        if strength == 1.:
            assert pure.tobytes() == patches.tobytes(), 'production purity 1 exact no-op'
    group('production purity: neutral/white bits, luminance, red formula, nonnegative, exact no-op', 0)
    start = len(failures)
    # Active Legacy and Camera variants. CPU selects gamma-free for type 8.
    programs = []
    for stage in (None, 'CAMERA'):
        for grade in (False, True):
            programs.append(P.camera(r, stage, grade))
    head = P.camera(r, None, ref='HEAD')
    round1 = [P.camera(r, stage, grade, ref='ROUND1') for stage in (None, 'CAMERA') for grade in (False, True)]
    head_error = 0.
    round1_cases = 0
    legacy_cases = 0
    neutral_codes = {}
    for strength in (.6, .9, 1.):
        for gamut, matrix in enumerate(mats):
            for video in (0, 1):
                for toe, ev in ((0., 0.), (.05, 1.), (.1, -1.)):
                    u = dict(base, tonemap_slog_purity=strength, tonemap_slog_gamut=gamut, tonemap_params=(0., float(video), toe, ev))
                    for index, pr in enumerate(programs):
                        codes = r.draw(pr, dst, w, 1, textures, u)[0][..., :3]
                        neutral_key = (pr, gamut, video, toe, ev)
                        neutral = codes[:, :5].tobytes()
                        assert neutral == neutral_codes.setdefault(neutral_key, neutral), 'neutral/white bit identity across purity'
                        full = (codes*1023.-64.)/876. if video else codes
                        recovered = ((slog3_inverse(full)-.18*toe) @ np.linalg.inv(matrix).T) / 2.**ev
                        recovered = inverse_purity(recovered, strength)
                        close(f'encode purity={strength} gamut={gamut} range={video} toe={toe} EV={ev} program={pr}',
                              recovered, patches, atol=DELIVERED_TOL, rtol=0)
                        if strength == 1.:
                            old_round1 = r.draw(round1[index], dst, w, 1, textures, u)[0][..., :3]
                            assert codes.tobytes() == old_round1.tobytes(), 'purity 1 round-1 byte identity'
                            round1_cases += 1
                        if gamut == 0 and strength == 1.:
                            old = r.draw(head, dst, w, 1, textures, u)[0][..., :3]
                            assert codes.tobytes() == old.tobytes(), 'legacy HEAD byte identity'
                            legacy_cases += 1
                            head_error = max(head_error, float(np.max(np.abs(recovered-patches))))
    group('production encode -> published S-Log3 inverse -> inverse matrix/purity; pure red; legacy HEAD bytes', start)
    assert 1e-5 < head_error <= DELIVERED_TOL, ('HEAD precision control', head_error)
    print(f'PASS GPU: HEAD precision floor {head_error:.10f}; half-code bound {DELIVERED_TOL:.10f}', flush=True)
    print(f'PASS GPU: legacy HEAD byte identity ({legacy_cases} cases); purity 1 round-1 byte identity ({round1_cases} cases)', flush=True)
    print('PASS GPU: neutral/white byte identity across all purity strengths and gamuts', flush=True)
    # Separate the gamut arithmetic from the inherited srgbF forward exponent
    # (0.41666 instead of 1/2.4). The full delivered-output test above remains
    # strict and fails independently; this tap must not hide its precision loss.
    start = len(failures)
    tap = r.program('class1/alchemy/colorCorrectF.glsl', '#define TONEMAP 1\n#define AL_SENSOR_CAMERA 1\n',
                    P.common_modules()+('class1/deferred/tonemapUtilF.glsl','class1/deferred/LPMUtil.glsl'),
                    mutation=('color = linear_to_srgb(color);', '// Test tap before downstream sRGB.'))
    for gamut, matrix in enumerate(mats):
        for video in (0, 1):
            u = dict(base, tonemap_slog_gamut=gamut, tonemap_params=(0., float(video), .05, 1.))
            linear = r.draw(tap, dst, w, 1, textures, u)[0][..., :3].astype(np.float64)
            exact_codes = np.where(linear < .0031308, linear*12.92, 1.055*linear**(1/2.4)-.055)
            full = (exact_codes*1023.-64.)/876. if video else exact_codes
            recovered = ((slog3_inverse(full)-.18*.05) @ np.linalg.inv(matrix).T)/2.
            close(f'encoder tap gamut={gamut} range={video}', recovered, patches, atol=3e-5, rtol=3e-5)
    group('production log/gamut encoder tap before inherited sRGB precision loss', start)
    start = len(failures)
    # Exercise the actual godray reconstruction with reachable non-neutral
    # increments. Its output must be Rec.709 light, before subsequent collection.
    scene = np.full_like(patches, .12)
    scene[0, 5:9] = np.array([[1., .01, .03], [.02, .8, .03], [.01, .02, 1.], [2., .03, .5]], np.float32)
    target = scene + np.array([.04, .03, .02], np.float32)
    source_tex = r.upload('scene', scene, fmt=0x8814)
    target_tex = r.upload('target', target, fmt=0x8814)
    camera = programs[2]
    for strength in (.6, .9, 1.):
        for gamut in range(4):
            for curve in range(4):
                for video in (0, 1):
                    u = dict(base, tonemap_slog_purity=strength, tonemap_slog_gamut=gamut, tonemap_params=(float(curve), float(video), .05, 1.))
                    def codes(texture):
                        return r.draw(camera, dst, w, 1,
                                      [('diffuseRect', texture), ('exposureMap', exp)], u)[0][..., :3]
                    ray = codes(target_tex)-codes(source_tex)
                    assert np.all(ray >= 0.)
                    ray_tex = r.upload('ray', ray, fmt=0x8814)
                    for grade in (False, True):
                        godray = P.camera(r, 'GODRAYS', grade)
                        out = r.draw(godray, dst, w, 1,
                                     [('diffuseRect', source_tex), ('diffuseMap', ray_tex), ('exposureMap', exp)], u)[0][..., :3]
                        if strength == 1.:
                            previous = P.camera(r, 'GODRAYS', grade, ref='ROUND1')
                            old = r.draw(previous, dst, w, 1,
                                         [('diffuseRect', source_tex), ('diffuseMap', ray_tex), ('exposureMap', exp)], u)[0][..., :3]
                            assert out.tobytes() == old.tobytes(), 'godray purity 1 round-1 byte identity'
                        close(f'godray purity={strength} gamut={gamut} curve={curve} range={video} grade={grade}',
                              out, target, atol=3e-5, rtol=3e-5)
    group('godray inverse all four curves/gamuts, both ranges and grade permutations', start)
    start = len(failures)
    hot = np.tile(np.array([70., .12, .12], np.float32), (1, w, 1))
    hot_tex = r.upload('hot', hot, fmt=0x8814)
    for gamut in (1, 2, 3):
        u = dict(base, tonemap_slog_gamut=gamut, tonemap_params=(0., 0., 0., 0.))
        before = r.draw(camera, dst, w, 1, [('diffuseRect', hot_tex), ('exposureMap', exp)], u)[0][..., :3]
        ray = np.broadcast_to(np.array([0., .0001, .0001], np.float32), hot.shape).copy()
        ray_tex = r.upload('hot_ray', ray, fmt=0x8814)
        out = r.draw(P.camera(r, 'GODRAYS'), dst, w, 1,
                     [('diffuseRect', hot_tex), ('diffuseMap', ray_tex), ('exposureMap', exp)], u)[0][..., :3]
        assert np.all(out[..., 0] > 69.), 'clipped highlight energy lost'
        out_tex = r.upload('hot_result', out, fmt=0x8814)
        after = r.draw(camera, dst, w, 1, [('diffuseRect', out_tex), ('exposureMap', exp)], u)[0][..., :3]
        close(f'godray clipped gamut={gamut}', after, np.clip(before+ray, 0., 1.), atol=3e-5, rtol=0)
    group('godray inverse retains original energy beyond clipped log codes', start)
    start = len(failures)
    # Link the production Prism entry points with a flat retained-camera signal.
    # No repo fixtures are rewritten. This catches per-program linking/uniform loss.
    stub = '''
in vec2 vary_fragcoord;
uniform sampler2D diffuseRect;
vec4 prismSignalSource(vec2 uv, vec3 p, vec3 n, bool incoming) { return texture(diffuseRect,uv); }
bool prismSignalSourceInFrame(vec2 uv, bool incoming) { return true; }
'''
    vertex = '''#version 420
in vec2 position;
out vec2 prism_uv;
out vec2 vary_fragcoord;
out vec3 prism_eye_pos;
out vec3 prism_eye_normal;
void main() { gl_Position=vec4(position,0,1); prism_uv=position*.5+.5;
vary_fragcoord=prism_uv; prism_eye_pos=vec3(0); prism_eye_normal=vec3(0,0,1); }
'''
    for path in ('prismPreviewF.glsl', 'prismProjectorFeedF.glsl'):
        for grade in (False, True):
            for look in ((False, True) if 'Feed' in path else (False,)):
                defs = '#define TONEMAP 1\n' + ('#define COLOR_GRADE 1\n' if grade else '') + ('#define PROJECTOR_LOOK 1\n' if look else '')
                modules = P.common_modules()+('class1/deferred/tonemapUtilF.glsl', 'class1/deferred/LPMUtil.glsl')
                if grade: modules += ('class1/alchemy/colorGradeUtilF.glsl',)
                def link_prism(ref=None):
                    program = r.gl.program()
                    for module in modules:
                        r.gl.attach(program, r.compile(r.source(module, ref), defs))
                    r.gl.attach(program, r.compile(stub + r.source('class1/deferred/'+path, ref), defs))
                    r.gl.attach(program, r.base.compile(vertex, 0x8B31))
                    r.gl.link(program)
                    ok = C.c_int()
                    r.gl.getprog(program, 0x8B82, C.byref(ok))
                    assert ok.value, 'Prism link '+path
                    return program
                pr, previous = link_prism(), link_prism('ROUND1')
                assert r.gl.loc(pr, b'tonemap_slog_gamut') >= 0
                assert r.gl.loc(pr, b'tonemap_slog_purity') >= 0
                for strength in (.6, .9, 1.):
                    for gamut, matrix in enumerate(mats):
                        u = dict(base, tonemap_slog_purity=strength, tonemap_slog_gamut=gamut, tonemap_params=(0., 0., 0., 0.),
                                 projectorFollowMainExposure=1, projectorLookLampGel=(1., 1., 1.))
                        out = r.draw(pr, dst, w, 1, textures, u)[0][..., :3]
                        if strength == 1.:
                            old = r.draw(previous, dst, w, 1, textures, u)[0][..., :3]
                            assert out.tobytes() == old.tobytes(), 'Prism purity 1 round-1 byte identity'
                        close(f'Prism purity={strength} {path} grade={grade} look={look} gamut={gamut}',
                              inverse_purity(slog3_inverse(out) @ np.linalg.inv(matrix).T, strength), patches, atol=DELIVERED_TOL, rtol=0)
    group('Prism preview/feed/look TONEMAP and grade variants', start)
    print('PASS GPU: purity 1 round-1 byte identity also covers 64 godray and 24 Prism cases', flush=True)
    output = ROOT/'.tmp/slog_gamut/gpu_failures.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(measurements, indent=2)+'\n', encoding='utf-8')
    if failures:
        print('\n'.join(failures[:5]))
        raise AssertionError(f'{len(failures)} GPU numerical failures (first five printed)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gpu', action='store_true')
    parser.add_argument('--gpu-worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    cpu()
    if args.gpu or args.gpu_worker:
        if not gpu_idle():
            print('PENDING-GPU: AlchemyTest.exe is running or process state is unavailable; rerun PYTHONUTF8=1 python scripts/code_tools/slog_gamut_check.py --gpu')
            return 2
        if args.gpu_worker:
            gpu()
        else:
            subprocess.run([sys.executable, __file__, '--gpu-worker'], timeout=240, check=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
