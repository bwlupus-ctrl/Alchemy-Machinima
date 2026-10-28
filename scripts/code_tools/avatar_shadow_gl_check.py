#!/usr/bin/env python3
"""Rasterize production avatar/legacy/PBR alpha casters into D32F using hidden WGL.

No viewer build or launch. Run with --gpu; checks AlchemyTest is absent before WGL.
The legacy/PBR fork-specific continuous-alpha policy and Actor FX are exercised too.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import relight_3d_gl as G

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / G.SHADERS
CHECKER = '0.5 * (floor(gl_FragCoord.x) + floor(gl_FragCoord.y))'


def viewer_closed():
    result = subprocess.run(['powershell', '-NoProfile', '-Command',
        "$ErrorActionPreference='Stop'; @(Get-Process -Name AlchemyTest -ErrorAction SilentlyContinue).Count"],
        capture_output=True, text=True, timeout=15)
    if result.returncode or result.stdout.strip() != '0':
        print('PENDING-GPU:', subprocess.list2cmdline([sys.executable, __file__, *sys.argv[1:]]),
              result.stdout, result.stderr)
        return False
    print('GPU lifecycle: Get-Process AlchemyTest count=0', flush=True)
    return True


def run(out):
    if not viewer_closed():
        return 3
    out.mkdir(parents=True, exist_ok=True)
    gl = G.GL()
    print('GL', gl.renderer, flush=True)
    assert not any(name in gl.renderer.lower() for name in
                   ('gdi generic', 'llvmpipe', 'softpipe', 'swiftshader', 'lavapipe', 'software', 'basic render')), 'hardware GPU required'
    enable = gl.fn('glEnable', None, gl.U)
    clear = gl.fn('glClear', None, gl.U)
    clear_depth = gl.fn('glClearDepth', None, C.c_double)
    depth_mask = gl.fn('glDepthMask', None, C.c_ubyte)
    depth_func = gl.fn('glDepthFunc', None, gl.U)
    draw_buffer = gl.fn('glDrawBuffer', None, gl.U)
    read_buffer = gl.fn('glReadBuffer', None, gl.U)
    const_attr = gl.fn('glVertexAttrib4f', None, gl.U, gl.F, gl.F, gl.F, gl.F)
    disable_attr = gl.fn('glDisableVertexAttribArray', None, gl.U)
    palette = gl.fn('glUniformMatrix3x4fv', None, gl.I, gl.I, C.c_ubyte, C.c_void_p)
    error = gl.fn('glGetError', gl.U)
    for cap in (0x0BE2, 0x0C11, 0x0B44, 0x864F, 0x8F9D):
        gl.disable(cap)
    enable(0x0B71)
    depth_mask(1)
    depth_func(0x0201)

    def texture(internal, w, h, fmt, data):
        tex = gl.U()
        gl.gen_tex(1, C.byref(tex))
        gl.bind_tex(0x0DE1, tex)
        for p, v in ((0x2801, 0x2600), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F)):
            gl.param(0x0DE1, p, v)
        gl.teximage(0x0DE1, 0, internal, w, h, 0, fmt, 0x1406,
                    None if data is None else data.ctypes.data)
        return tex

    depth = texture(0x8CAC, 264, 28, 0x1902, None)
    fbo = gl.U()
    gl.gen_fbo(1, C.byref(fbo))
    gl.bind_fbo(0x8D40, fbo)
    gl.fbo_tex(0x8D40, 0x8D00, 0x0DE1, depth, 0)
    draw_buffer(0)
    read_buffer(0)
    assert gl.fbo_status(0x8D40) == 0x8CD5, 'D32F target'
    ramp = np.ones((1, 256, 4), np.float32)
    ramp[0, :, 3] = np.arange(256, dtype=np.float32) / 255
    diffuse = texture(0x8814, 256, 1, 0x1908, ramp)
    vbo = gl.U()
    gl.gen_buf(1, C.byref(vbo))
    # Production vertices, UVs and real skin palettes, not a replacement vertex main.
    vertices = np.array([[-1, -1, 0, 0, 0], [3, -1, 0, 2, 0], [-1, 3, 0, 0, 2]], np.float32)
    gl.bind_buf(0x8892, vbo)
    gl.buffer_data(0x8892, vertices.nbytes, vertices.ctypes.data, 0x88E4)
    objects = {}

    def compile_shader(text, defines, stage, label):
        key = text, defines, stage
        if key in objects:
            return objects[key]
        prefix = '#version 420\n' + G.PREAMBLE + defines
        prefix += '#define VERTEX_SHADER 1\n' if stage == 0x8B31 else '#define FRAGMENT_SHADER 1\n'
        source = (prefix + text).encode()
        obj = gl.create_shader(stage)
        ptr = C.c_char_p(source)
        gl.source(obj, 1, C.byref(ptr), None)
        gl.compile(obj)
        ok = gl.I()
        gl.getshader(obj, 0x8B81, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.slog(obj, len(log), None, log)
            raise AssertionError(label + ' COMPILE ' + log.value.decode())
        objects[key] = obj
        return obj

    def program(kind, rigged=False, fullbright=False, column=False):
        avatar = kind.startswith('avatar')
        vertex = 'avatarAlphaShadowV' if avatar else 'pbrShadowAlphaMaskV' if kind == 'pbr' else 'shadowAlphaMaskV'
        fragment = dict(avatar_blend='avatarAlphaShadowF', avatar_mask='avatarAlphaMaskShadowF',
                        legacy='shadowAlphaMaskF', pbr='pbrShadowAlphaBlendF')[kind]
        defines = ('#define HAS_SKIN 1\n' if rigged else '')
        defines += '#define IS_FULLBRIGHT 1\n#define DEPTH_CLAMP 1\n' if fullbright else ''
        modules = [('class1/deferred/' + vertex + '.glsl', 0x8B31),
                   ('class1/deferred/' + fragment + '.glsl', 0x8B30),
                   ('class1/objects/nonindexedTextureV.glsl', 0x8B31),
                   ('class1/alchemy/actorFxDissolveF.glsl', 0x8B30)]
        if avatar or rigged:
            modules.append(('class1/avatar/' + ('avatarSkinV' if avatar else 'objectSkinV') + '.glsl', 0x8B31))
        if kind == 'pbr':
            modules.append(('class1/deferred/textureUtilV.glsl', 0x8B31))
        pr = gl.program()
        for path, stage in modules:
            text = (SHADERS / path).read_text(encoding='utf-8')
            if column and path.endswith(fragment + '.glsl'):
                assert text.count(CHECKER) == 1
                text = text.replace(CHECKER, '0.5 * floor(gl_FragCoord.x)')
            gl.attach(pr, compile_shader(text, defines, stage, path))
        if kind == 'legacy':
            # LLShaderMgr's single diffuse channel glue (same as the existing GL harness).
            glue = 'uniform sampler2D tex0; vec4 diffuseLookup(vec2 uv){return texture(tex0,uv);}'
            gl.attach(pr, compile_shader(glue, defines, 0x8B30, 'diffuseLookup'))
        gl.link(pr)
        ok = gl.I()
        gl.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.plog(pr, len(log), None, log)
            raise AssertionError(kind + ' LINK ' + log.value.decode())
        return pr

    def draw(pr, minimum, offset, mode=0, dissolve=False):
        gl.use(pr)
        for name in ('texture_matrix0', 'projection_matrix', 'modelview_matrix', 'modelview_projection_matrix'):
            m = np.eye(4, dtype=np.float32)
            gl.matrix(gl.loc(pr, name.encode()), 1, 0, m.ctypes.data_as(C.POINTER(gl.F)))
        avatar_palette = np.zeros((45, 4), np.float32)
        avatar_palette[[0, 15, 30]] = np.eye(4, dtype=np.float32)[:3]
        loc = gl.loc(pr, b'matrixPalette')
        # Avatar palettes have vec4[45]; object palettes have mat3x4[110].
        index = gl.attr_loc(pr, b'weight')
        if index >= 0:
            gl.u4fv(loc, 45, avatar_palette.ctypes.data_as(C.POINTER(gl.F)))
        elif loc >= 0:
            m = np.eye(4, dtype=np.float32)[:3].copy()
            palette(loc, 1, 0, m.ctypes.data)
        for name, value in ((b'weight', (0, 0, 0, 0)), (b'weight4', (.5, 0, 0, 0)),
                            (b'diffuse_color', (1, 1, 1, 1)), (b'normal', (0, 0, 1, 0))):
            idx = gl.attr_loc(pr, name)
            if idx >= 0:
                disable_attr(idx)
                const_attr(idx, *value)
        for name, count, start in ((b'position', 3, 0), (b'texcoord0', 2, 12)):
            idx = gl.attr_loc(pr, name)
            assert idx >= 0, name
            gl.enable_attr(idx)
            gl.attr_ptr(idx, count, 0x1406, 0, 20, C.c_void_p(start))
        gl.uf[1](gl.loc(pr, b'minimum_alpha'), minimum)
        gl.uf[1](gl.loc(pr, b'shadow_target_width'), 0)  # parity must not depend on this
        gl.uf[4](gl.loc(pr, b'color'), 1, 1, 1, 1)
        transform = np.array([[1, 1, 0, 0], [0, 0, 0, 0]], np.float32)
        gl.u4fv(gl.loc(pr, b'texture_base_color_transform'), 2, transform.ctypes.data_as(C.POINTER(gl.F)))
        gl.ui(gl.loc(pr, b'shadow_alpha_mode'), mode)
        gl.ui(gl.loc(pr, b'shadow_alpha_pass'), 1)
        gl.uf[1](gl.loc(pr, b'shadow_alpha_cutoff'), .25)
        gl.ui(gl.loc(pr, b'actorFxEnabled'), int(dissolve))
        gl.ui(gl.loc(pr, b'actorFxLook'), 10)
        gl.uf[1](gl.loc(pr, b'actorFxDissolveProgress'), 1)
        gl.active(0x84C0)
        gl.bind_tex(0x0DE1, diffuse)
        for sampler in (b'diffuseMap', b'tex0'):
            gl.ui(gl.loc(pr, sampler), 0)
        gl.viewport(*offset, 256, 16)
        clear_depth(1)
        clear(0x100)
        gl.draw(4, 0, 3)
        data = np.empty((16, 256), np.float32)
        gl.read(*offset, 256, 16, 0x1902, 0x1406, data.ctypes.data)
        assert error() == 0, 'GL error'
        assert np.all((data == 1) | (data == .5)), 'unexpected depth'
        return data < 1

    def verify(actual, minimum, stipple, offset):
        alpha = np.broadcast_to(ramp[0, :, 3], actual.shape)
        assert not actual[alpha < minimum].any(), 'nothing below min'
        expected = alpha >= minimum
        if stipple:
            yy, xx = np.indices(actual.shape)
            parity = (xx + offset[0] + yy + offset[1]) % 2 == 1
            expected &= (alpha >= np.float32(.88)) | parity
            mid = (alpha >= minimum) & (alpha < np.float32(.88))
            assert np.mean(actual[mid]) == .5, '50% density'
        assert np.array_equal(actual, expected), 'checkerboard parity' if stipple else 'solid mask'

    rows = []
    for kind, rigged, fullbright in [('avatar_blend', False, False), ('avatar_mask', False, False),
            ('legacy', False, False), ('legacy', True, False), ('legacy', False, True),
            ('legacy', True, True), ('pbr', False, False), ('pbr', True, False)]:
        pr = program(kind, rigged, fullbright)
        # PBR mode 0 intentionally retains the fork's .05 cutoff (no minimum_alpha discard).
        minimum = .05 if kind == 'pbr' else .2
        for offset in ((0, 0), (3, 5), (4, 5)):
            verify(draw(pr, minimum, offset), minimum, kind != 'avatar_mask', offset)
        assert not draw(pr, minimum, (3, 5), dissolve=True).any(), 'Actor FX dissolve coverage'
        if kind in ('legacy', 'pbr'):
            effective = .25 if kind == 'pbr' else .2
            verify(draw(pr, minimum, (3, 5), mode=1), effective, False, (3, 5))
        if kind.startswith('avatar'):
            try:
                verify(draw(pr, 0, (3, 5)), .2, kind != 'avatar_mask', (3, 5))
            except AssertionError as exc:
                assert str(exc) == 'nothing below min', str(exc)
            else:
                raise AssertionError('minimum_alpha=0 negative control survived')
        if kind != 'avatar_mask':
            mutated = program(kind, rigged, fullbright, column=True)
            try:
                verify(draw(mutated, minimum, (3, 5)), minimum, True, (3, 5))
            except AssertionError as exc:
                assert str(exc) == 'checkerboard parity', str(exc)
            else:
                raise AssertionError('column-stipple negative control survived')
        row = dict(kind=kind, rigged=rigged, fullbright=fullbright, passed=True,
                   zero_minimum_rejected=kind.startswith('avatar'),
                   column_stipple_rejected=kind != 'avatar_mask')
        rows.append(row)
        print('PASS', row, flush=True)
    (out / 'results.json').write_text(json.dumps(dict(renderer=gl.renderer, rows=rows), indent=2) + '\n', encoding='utf-8')
    print('PASS production D32F alpha ramps, exact fragment parity, cutoffs, continuous mode, Actor FX, negative controls')
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gpu', action='store_true', required=True)
    parser.add_argument('--out', type=Path, default=ROOT / '.tmp/avatar_shadow_20261028/gpu')
    args = parser.parse_args()
    sys.exit(run(args.out))
