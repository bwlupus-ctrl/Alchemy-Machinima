"""Additive production-shader regressions for clone FX fix round 2."""
import ctypes as C
import json
import re

import numpy as np

from clone_looks_gl_check import Runner, F, V, SH, ROOT

SYSTEM = SH + 'avatar/avatarActorGhostV.glsl'
EYE = SH + 'avatar/eyeballActorGhostV.glsl'


def source_assertions(tree, block):
    assert 'floatBitsToUint' not in block
    assert 'p = clamp(p, vec2(-262144.0), vec2(262143.0));' in block
    # Check both conversion and signed addition, including noncanonical meshes.
    quantized = np.floor(np.array([-262144., 262143.], np.float32) * np.float32(4096.)).astype(np.int64)
    shifted = quantized + 0x40000000
    assert np.all(quantized >= -(2**31)) and np.all(quantized < 2**31)
    assert np.all(shifted >= -(2**31)) and np.all(shifted < 2**31)
    assert 'float slab = floor((fxHeight + 0.11) / 0.22);' in block
    fs = tree.read(F)
    assert 'float slabZ = mod(P.z + 0.11, 0.22);' in fs
    eye = float(re.search(r'const float sl_eye = ([0-9.]+);', fs)[1])
    fraction = (eye + .11) % .22
    assert min(fraction, .22 - fraction) >= .08, 'eye height too close to a Slice boundary'
    assert 'if (vary_fx_body == 2) cloneFxWipeEdge = 0.0;' in fs
    assert 'cloneFxDim = 1.0 - 2.0 * amount;' in fs
    assert 'min(cloneFxDim, 1.0 - 0.75 * amount)' in fs
    assert 'alpha *= cloneFxDim;' in fs
    assert fs.index('alpha *= cloneFxDim;') < fs.index('frag_color = vec4(clamp(alpha, 0.0, 1.0));')
    assert fs.count('float worldSignal = flicker * max(ghostFx.w, 0.0);') == 1
    assert '''#ifdef GHOST_WORLD_PASS
        worldSignal *= cloneFxDim;
        worldRadiance *= cloneFxDim;
        worldBloomRadiance *= cloneFxDim;
#else
        rgb *= cloneFxDim;
#endif''' in fs
    assert 'cloneFxWipeEdge * 0.75) * cloneFxDim' not in fs


def compile_versions(tree, gl):
    rows = []
    for version in (140, 150, 330):
        for vertex, defs in ((V, ()), (V, ('HAS_SKIN',)),
                             (SYSTEM, ()), (SYSTEM, ('AVATAR_CLOTH',)), (EYE, ())):
            source = tree.read(vertex)
            if 'HAS_SKIN' in defs:
                source += tree.read(SH + 'avatar/objectSkinV.glsl')
            if vertex == SYSTEM:
                source += tree.read(SH + 'avatar/avatarSkinV.glsl')
            if vertex in (SYSTEM, EYE):
                source += '\nvoid calcAtmospherics(vec3 p) {}\n'
            header = f'#version {version}\n#define MAX_JOINTS_PER_MESH_OBJECT 110\n'
            header += ''.join(f'#define {d} 1\n' for d in defs)
            shader = gl.create_shader(0x8B31)
            raw = C.c_char_p((header + source).encode())
            gl.source(shader, 1, C.byref(raw), None)
            gl.compile(shader)
            ok = gl.I()
            gl.getshader(shader, 0x8B81, C.byref(ok))
            log = C.create_string_buffer(64000)
            gl.slog(shader, len(log), None, log)
            assert ok.value, (version, vertex, defs, log.value.decode())
            gl.fn('glDeleteShader', None, gl.U)(shader)
            rows.append(dict(version=version, vertex=vertex, defines=defs, passed=True))
    print('PASS fix2: full production vertex compile at GLSL 140/150/330 (15 variants)', flush=True)
    return rows


def check(tree, gl, real, real_program):
    from clone_looks_safety_check import save
    from PIL import Image
    out = ROOT / '.tmp/looks16/renders/clone_fx_fix2'
    out.mkdir(parents=True, exist_ok=True)
    result = dict(vertex_compiles=compile_versions(tree, gl))
    r = Runner(96, 96, gl)
    normal = r.program(tree, ('HAS_SKIN',))
    key = r.program(tree, ('HAS_SKIN', 'AL_KEY_COVERAGE_OUT'))
    # The exact centre of a .45 core window, verified against real shader cuts.
    phase = .73
    hit = next(t for slot in range(24)
               if (t := (slot + .2475) / 2 - phase * .73) >= 0
               and not np.any(r.render(normal, 1, time=t, phase=phase,
                                       fx2=(0, 0, .5, 0), mask=False)))
    result['power_mid_window_time'] = hit
    for program in (normal, key):
        baseline = r.render(program, 1, time=hit, mask=False)
        covered = baseline[..., 3] > 0
        assert covered.sum() > 100
        for amount in (.25, .45, .49, .499, .4999):
            frame = r.render(program, 1, time=hit, fx2=(0, 0, amount, 0), mask=False)
            dim = 1 - 2 * amount
            assert np.any(frame[..., 3]), ('sub-.5 flicker empty', amount)
            assert np.max(np.abs(frame[..., 3] - baseline[..., 3] * dim)) < 2e-6, ('alpha/key continuity', amount)
            assert np.max(np.abs(frame[..., :3] - baseline[..., :3] * dim)) < 2e-6, ('RGB/key continuity', amount)
        assert not np.any(r.render(program, 1, time=hit, fx2=(0, 0, .5, 0), mask=False))
    result['core_rgb_alpha_key_continuity'] = True
    world = ('HAS_SKIN', 'GHOST_WORLD_PASS', 'GHOST_SHARED_DISSOLVE')

    class Isolate:
        def __init__(self, glow): self.glow = glow
        def read(self, path):
            source = tree.read(path)
            if path == F:
                if self.glow:
                    assert source.count('if (ghostGlowOnly != 0)') == 1
                    source = source.replace('if (ghostGlowOnly != 0)', 'if (true)')
                else:
                    assert source.count('rgb += worldRadiance;') == 1
                    source = source.replace('rgb += worldRadiance;', 'rgb += vec3(0.0);')
            return source

    # Isolate production body and bloom outputs, with identical linear/fog paths.
    body = r.program(Isolate(False), world)
    glow = r.program(Isolate(True), world)
    values = []
    for program, channel in ((body, slice(0, 3)), (glow, 3)):
        a = r.render(program, 30, time=hit, mask=False)
        b = r.render(program, 30, time=hit, mask=False, fx2=(0, 0, .45, 0))
        signal, dimmed = float(a[..., channel].sum()), float(b[..., channel].sum())
        assert signal > .01 and dimmed > 0, 'body/glow ratio fixture empty'
        assert abs(dimmed / signal - .1) < 2e-6, ('linear dim law', channel, dimmed / signal)
        values.append((signal, dimmed))
    ratio_before = values[0][0] / values[1][0]
    ratio_after = values[0][1] / values[1][1]
    assert abs(ratio_after / ratio_before - 1) < 2e-5, 'body-to-glow ratio changed during dim'
    result['world_body_glow'] = dict(body_attenuation=values[0][1]/values[0][0],
                                   glow_attenuation=values[1][1]/values[1][0],
                                   ratio_before=ratio_before, ratio_during=ratio_after)
    original_eye = real.eye_program
    real.camera_center_z = 1.762
    real.only = {'mEyeLeft', 'mEyeRight'}
    baseline = real.frame(real_program, 1, phase=phase, scale=2.8)
    assert np.any(baseline[..., 3]), 'native eyes missing'
    # Immediately before the eye plane: no whole-eyeball cyan flash.
    wipe_edge = (1.762 - .003 - .006) / (1.873 - .006)
    edge = real.frame(real_program, 1, phase=phase, scale=2.8, fx2=(0, 0, 0, wipe_edge))
    assert np.array_equal(baseline, edge), 'Wipe edge tints native eyes'
    wipe_after = (1.762 + .00001 - .006) / (1.873 - .006)
    assert not np.any(real.frame(real_program, 1, phase=phase, scale=2.8, fx2=(0, 0, 0, wipe_after))), 'eye plane leaves eyes'
    result['eye_wipe_no_tint'] = True
    wipe_at_eye = (1.762 - .006) / (1.873 - .006)
    result['wipe_render_amount'] = wipe_at_eye
    real.only = {'head', 'upper_body', 'lower_body', 'mEyeLeft', 'mEyeRight'}
    save(real.frame(real_program, 1, phase=phase, scale=2.8, fx2=(0, 0, 0, wipe_at_eye)), out/'wipe_eye_plane.png')
    # Slice at a strong offset; fixed source asserts put eyes 108 mm from a cut.
    save(real.frame(real_program, 1, time=4.27, scale=2.8, fx2=(1, 0, 0, 0)), out/'slice_1_head_close.png')
    real.camera_center_z = sum(real.pose_bounds['bind']) * .5
    def save_power(program, look, name, linear=False):
        frame = real.frame(program, look, time=hit, phase=phase, scale=.92, fx2=(0, 0, .45, 0))
        assert np.isfinite(frame).all() and np.any(frame[..., 3] > .01), 'native .45 flicker render empty'
        # Native draws already use premultiplied framebuffer compositing. Show
        # the dark actor over a neutral background; encode linear world output
        # for an ordinary sRGB PNG, without exposure or signal normalization.
        rgb = frame[..., :3] + .18 * (1 - frame[..., 3:4])
        if linear:
            rgb = np.where(rgb <= .0031308, rgb * 12.92, 1.055 * np.maximum(rgb, 0)**(1/2.4) - .055)
        Image.fromarray((np.clip(rgb[::-1], 0, 1)*255+.5).astype(np.uint8)).save(out/name)
    save_power(real_program, 1, 'power_045_mid_window_normal.png')
    wp = real.program(tree, world)
    real.eye_program = Runner.program(real, tree, ('GHOST_WORLD_PASS', 'GHOST_SHARED_DISSOLVE'), EYE)
    save_power(wp, 30, 'power_045_mid_window_world.png', linear=True)
    result['power_png_presentation'] = dict(background=.18, normal='display space', world='linear composited, then sRGB encoded', exposure=1)
    real.eye_program = original_eye
    real.camera_center_z = 1.76
    (out/'fix2_metrics.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('PASS fix2: eye boundary clearance/tint, .5 RGB/alpha/key continuity, linear body-to-glow ratio, native renders', flush=True)
    return result
