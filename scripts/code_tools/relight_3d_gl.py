"""Hidden-WGL runner for the PRODUCTION area-shadow gather + bilateral filter/upsample shaders.

Same approach as relight_shader_check.py (hidden window, legacy WGL context, shader modules concatenated like
LLShaderMgr::attachShaderFeatures), but standalone and with the shader source read from a git ref
(`--shader-ref`, default HEAD) so a concurrently edited working tree never leaks in. `WORKTREE` reads the disk.
"""
import ctypes as C
import json
import os
import subprocess
import sys
from ctypes import wintypes as W
from functools import lru_cache
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
SHADERS = 'indra/newview/app_settings/shaders'


def pack_view_rotation(view):
    """The SAME rule as alworldshadowmath.h packViewRotation: out[3*column + row] = glmView[column][row]. `view` is the harness's row-major 4x4
    (view[row, column]), so glm's [column][row] is view[row, column]. Pinned against the C++ test vector by relight_world_check.py."""
    out = np.zeros(9, np.float32)
    for column in range(3):
        for row in range(3):
            out[3 * column + row] = view[row][column]
    return out


class SourceTree:
    """Read repo files at a git ref (or the working tree for ref == 'WORKTREE')."""

    def __init__(self, ref):
        self.ref = ref
        self._listing = None

    def read(self, path):
        path = Path(path).as_posix()
        if self.ref == 'WORKTREE':
            return (REPO / path).read_text(encoding='utf-8')
        return subprocess.check_output(['git', '-C', str(REPO), 'show', '%s:%s' % (self.ref, path)],
                                       text=True, encoding='utf-8')

    def exists(self, path):
        path = Path(path).as_posix()
        if self.ref == 'WORKTREE':
            return (REPO / path).exists()
        if self._listing is None:
            out = subprocess.check_output(['git', '-C', str(REPO), 'ls-tree', '-r', '--name-only', self.ref, '--', SHADERS],
                                          text=True, encoding='utf-8')
            self._listing = set(out.split('\n'))
        return path in self._listing

    def commit(self):
        if self.ref == 'WORKTREE':
            return 'WORKTREE'
        return subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', '--short=11', self.ref], text=True).strip()


PREAMBLE = """
#define MAX_JOINTS_PER_MESH_OBJECT 110
#define REF_SAMPLE_COUNT 32
#define GBUFFER_FLAG_HAS_PBR 0.67
#define GBUFFER_FLAG_HAS_HDRI 1.0
#define GBUFFER_FLAG_HAS_ATMOS 0.34
#define GBUFFER_FLAG_SKIP_ATMOS 0.0
#define GET_GBUFFER_FLAG(data, flag) (abs(data-flag)<0.1)
#define texture2D texture
#define textureCube texture
vec4 diffuseLookup(vec2 uv);
struct GBufferInfo { vec4 albedo; vec4 specular; vec3 normal; vec4 emissive; float gbufferFlag; float envIntensity; };
"""


class GL:
    """Minimal ctypes OpenGL context + wrappers."""

    def __init__(self):
        user = C.WinDLL('user32')
        gdi = C.WinDLL('gdi32')
        ogl = C.WinDLL('opengl32')
        self.ogl = ogl

        def api(dll, name, result, *args):
            f = getattr(dll, name)
            f.restype = result
            f.argtypes = args
            return f
        create = api(user, 'CreateWindowExW', W.HWND, W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, C.c_int, C.c_int,
                     C.c_int, C.c_int, W.HWND, W.HMENU, W.HINSTANCE, C.c_void_p)
        wnd = create(0, 'STATIC', 'Relight 3D reference', 0, 0, 0, 16, 16, None, None, None, None)
        assert wnd, 'hidden WGL window'
        dc = api(user, 'GetDC', W.HDC, W.HWND)(wnd)

        class PFD(C.Structure):
            _fields_ = [('nSize', W.WORD), ('nVersion', W.WORD), ('dwFlags', W.DWORD)] + \
                [(n, C.c_ubyte) for n in ['iPixelType', 'cColorBits', 'cRedBits', 'cRedShift', 'cGreenBits', 'cGreenShift',
                                          'cBlueBits', 'cBlueShift', 'cAlphaBits', 'cAlphaShift', 'cAccumBits',
                                          'cAccumRedBits', 'cAccumGreenBits', 'cAccumBlueBits', 'cAccumAlphaBits',
                                          'cDepthBits', 'cStencilBits', 'cAuxBuffers', 'iLayerType', 'bReserved']] + \
                [(n, W.DWORD) for n in ['dwLayerMask', 'dwVisibleMask', 'dwDamageMask']]
        pfd = PFD()
        pfd.nSize = C.sizeof(pfd)
        pfd.nVersion = 1
        pfd.dwFlags = 0x25
        pfd.cColorBits = 32
        pfd.cAlphaBits = 8
        fmt = api(gdi, 'ChoosePixelFormat', C.c_int, W.HDC, C.POINTER(PFD))(dc, C.byref(pfd))
        assert fmt
        assert api(gdi, 'SetPixelFormat', W.BOOL, W.HDC, C.c_int, C.POINTER(PFD))(dc, fmt, C.byref(pfd))
        ctx = api(ogl, 'wglCreateContext', C.c_void_p, W.HDC)(dc)
        assert ctx
        assert api(ogl, 'wglMakeCurrent', W.BOOL, W.HDC, C.c_void_p)(dc, ctx)
        self._getproc = api(ogl, 'wglGetProcAddress', C.c_void_p, C.c_char_p)
        self._api = lambda name, result, *args: api(ogl, name, result, *args)
        I, U, F, P = C.c_int, C.c_uint, C.c_float, C.c_void_p
        self.I, self.U, self.F, self.P = I, U, F, P
        g = self.fn
        getstr = g('glGetString', C.c_char_p, U)
        self.renderer = '%s %s' % (getstr(0x1F02).decode(), getstr(0x1F01).decode())
        self.create_shader = g('glCreateShader', U, U)
        self.source = g('glShaderSource', None, U, I, C.POINTER(C.c_char_p), C.POINTER(I))
        self.compile = g('glCompileShader', None, U)
        self.getshader = g('glGetShaderiv', None, U, U, C.POINTER(I))
        self.slog = g('glGetShaderInfoLog', None, U, I, C.POINTER(I), C.c_char_p)
        self.program = g('glCreateProgram', U)
        self.attach = g('glAttachShader', None, U, U)
        self.link = g('glLinkProgram', None, U)
        self.getprog = g('glGetProgramiv', None, U, U, C.POINTER(I))
        self.plog = g('glGetProgramInfoLog', None, U, I, C.POINTER(I), C.c_char_p)
        self.use = g('glUseProgram', None, U)
        self.loc = g('glGetUniformLocation', I, U, C.c_char_p)
        self.uf = [None] + [g('glUniform%df' % n, None, I, *([F] * n)) for n in range(1, 5)]
        self.ui = g('glUniform1i', None, I, I)
        self.matrix = g('glUniformMatrix4fv', None, I, I, U, C.POINTER(F))
        self.u4fv = g('glUniform4fv', None, I, I, C.POINTER(F))
        self.u4uiv = g('glUniform4uiv', None, I, I, C.POINTER(U))
        self.u3fv = g('glUniform3fv', None, I, I, C.POINTER(F))
        self.gen_tex = g('glGenTextures', None, I, C.POINTER(U))
        self.del_tex = g('glDeleteTextures', None, I, C.POINTER(U))
        self.bind_tex = g('glBindTexture', None, U, U)
        self.teximage = g('glTexImage2D', None, U, I, I, I, I, I, U, U, P)
        self.param = g('glTexParameteri', None, U, U, I)
        self.active = g('glActiveTexture', None, U)
        self.gen_fbo = g('glGenFramebuffers', None, I, C.POINTER(U))
        self.bind_fbo = g('glBindFramebuffer', None, U, U)
        self.fbo_tex = g('glFramebufferTexture2D', None, U, U, U, U, I)
        self.fbo_status = g('glCheckFramebufferStatus', U, U)
        self.viewport = g('glViewport', None, I, I, I, I)
        self.disable = g('glDisable', None, U)
        self.read = g('glReadPixels', None, I, I, I, I, U, U, P)
        self.finish = g('glFinish', None)
        self.gen_buf = g('glGenBuffers', None, I, C.POINTER(U))
        self.bind_buf = g('glBindBuffer', None, U, U)
        self.buffer_data = g('glBufferData', None, U, C.c_ssize_t, P, U)
        self.enable_attr = g('glEnableVertexAttribArray', None, U)
        self.attr_ptr = g('glVertexAttribPointer', None, U, I, U, U, I, P)
        self.attr_loc = g('glGetAttribLocation', I, U, C.c_char_p)
        self.draw = g('glDrawArrays', None, U, I, I)
        self.color_mask = g('glColorMask', None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)
        self.pixel_store = g('glPixelStorei', None, U, I)
        self.gen_queries = g('glGenQueries', None, I, C.POINTER(U))
        self.begin_query = g('glBeginQuery', None, U, U)
        self.end_query = g('glEndQuery', None, U)
        self.query_u64 = g('glGetQueryObjectui64v', None, U, U, C.POINTER(C.c_uint64))

    def fn(self, name, result, *args):
        ptr = self._getproc(name.encode())
        if ptr and ptr not in (1, 2, 3, 2 ** 64 - 1):
            return C.WINFUNCTYPE(result, *args)(ptr)
        return self._api(name, result, *args)


class AreaRunner:
    """Links the production areaShadowF / areaShadowFilterF programs from a source tree and runs the 3-pass pipeline
    exactly like LLPipeline::renderAreaShadows (gather at trace res -> separable blur -> bilateral upsample)."""

    FRAGMENT_COMMON = ['deferred/globalF.glsl', 'environment/srgbF.glsl', 'windlight/atmosphericsVarsF.glsl',
                       'windlight/gammaF.glsl', 'windlight/atmosphericsFuncs.glsl', 'windlight/atmosphericsF.glsl',
                       'deferred/deferredUtil.glsl', 'deferred/gbufferUtil.glsl']
    LEVEL1 = ('deferred/deferredUtil.glsl', 'deferred/gbufferUtil.glsl', 'deferred/globalF.glsl', 'deferred/shadowUtil.glsl')

    def __init__(self, tree, gl=None):
        self.tree = tree
        self.gl = gl or GL()  # a caller comparing two source trees shares ONE context
        gl = self.gl
        self._cache = {}
        self.gather = self._link('class3/deferred/areaShadowF.glsl')
        self.filter = self._link('class3/deferred/areaShadowFilterF.glsl')
        for cap in [0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0]:
            gl.disable(cap)
        vbo = C.c_uint()
        gl.gen_buf(1, C.byref(vbo))
        gl.bind_buf(0x8892, vbo)
        verts = np.array([-1, -1, 0, 3, -1, 0, -1, 3, 0], np.float32)
        gl.buffer_data(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
        self.vbo = vbo
        self.fbo = C.c_uint()
        gl.gen_fbo(1, C.byref(self.fbo))
        self.textures = {}
        gl.pixel_store(0x0D05, 1)   # GL_PACK_ALIGNMENT
        gl.pixel_store(0x0CF5, 1)   # GL_UNPACK_ALIGNMENT

    # ---- shader assembly (same rules as relight_shader_check.link_production)
    def _production_file(self, name, level=3):
        for cls in range(level, 0, -1):
            cand = '%s/class%d/%s' % (SHADERS, cls, name)
            if self.tree.exists(cand):
                return cand
        raise AssertionError(name)

    def _compile(self, text, label, stage=0x8B30):
        gl = self.gl
        sh = gl.create_shader(stage)
        injection = PREAMBLE + '\n'
        prepared = text.replace('/*[EXTRA_CODE_HERE]*/', injection, 1) if '/*[EXTRA_CODE_HERE]*/' in text else injection + text
        raw = ('#version 420\n' + prepared).encode()
        src = C.c_char_p(raw)
        gl.source(sh, 1, C.byref(src), None)
        gl.compile(sh)
        ok = C.c_int()
        gl.getshader(sh, 0x8B81, C.byref(ok))
        if not ok.value:
            out = C.create_string_buffer(32000)
            gl.slog(sh, len(out), None, out)
            raise AssertionError(label + ' ' + out.value.decode())
        return sh

    def _link(self, file, subs=()):
        gl = self.gl
        name = Path(file).name
        fragment = list(self.FRAGMENT_COMMON) + ['deferred/screenSpaceRayUtil.glsl']
        vertices = ['deferred/areaShadowV.glsl', 'deferred/textureUtilV.glsl', 'objects/nonindexedTextureV.glsl']
        pr = gl.program()
        for module in fragment:
            path = self._production_file(module, 1 if module in self.LEVEL1 else 3)
            gl.attach(pr, self._compile(self.tree.read(path), name + ' ' + module))
        text = self.tree.read(SHADERS + '/' + file)
        for old, new in subs:  # diagnostic shader variants only (harness experiments), never production text
            assert old in text, old
            text = text.replace(old, new)
        gl.attach(pr, self._compile(text, name))
        for module in vertices:
            path = self._production_file(module)
            gl.attach(pr, self._compile(self.tree.read(path), name + ' ' + module, 0x8B31))
        gl.link(pr)
        ok = C.c_int()
        gl.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            out = C.create_string_buffer(64000)
            gl.plog(pr, len(out), None, out)
            raise AssertionError(name + ' LINK ' + out.value.decode())
        return pr

    # ---- helpers
    def _timed(self, key, stats, fn):
        if stats is None:
            fn()
            return
        gl = self.gl
        q = C.c_uint()
        gl.gen_queries(1, C.byref(q))
        gl.begin_query(0x88BF, q)
        fn()
        gl.end_query(0x88BF)
        gl.finish()
        r = C.c_uint64(0)
        gl.query_u64(q, 0x8866, C.byref(r))
        stats[key] = stats.get(key, 0.0) + r.value * 1.e-6

    def _texture(self, key, unit, internal, w, h, fmt, typ, data, nearest=True):
        gl = self.gl
        tex = self.textures.get(key)
        if tex is None or tex[1:] != (w, h, internal):
            if tex is not None:
                gl.del_tex(1, C.byref(tex[0]))
            t = C.c_uint()
            gl.gen_tex(1, C.byref(t))
            tex = (t, w, h, internal)
            self.textures[key] = tex
        gl.active(0x84C0 + unit)
        gl.bind_tex(0x0DE1, tex[0])
        gl.param(0x0DE1, 0x2801, 0x2600)
        gl.param(0x0DE1, 0x2800, 0x2600)
        gl.param(0x0DE1, 0x2802, 0x812F)
        gl.param(0x0DE1, 0x2803, 0x812F)
        gl.teximage(0x0DE1, 0, internal, w, h, 0, fmt, typ, data.ctypes.data if data is not None else None)
        return tex[0]

    def _mat(self, pr, name, M):
        v = np.ascontiguousarray(np.asarray(M, np.float64).T, np.float32)
        self.gl.matrix(self.gl.loc(pr, name), 1, 0, v.ctypes.data_as(C.POINTER(self.gl.F)))

    def _begin(self, pr):
        gl = self.gl
        gl.use(pr)
        index = gl.attr_loc(pr, b'position')
        assert index >= 0
        gl.bind_buf(0x8892, self.vbo)
        gl.enable_attr(index)
        gl.attr_ptr(index, 3, 0x1406, 0, 0, None)

    def _target(self, key, w, h):
        gl = self.gl
        tex = self._texture(key, 0, 0x8058, w, h, 0x1908, 0x1401, None)
        gl.bind_fbo(0x8D40, self.fbo)
        gl.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, tex, 0)
        assert gl.fbo_status(0x8D40) == 0x8CD5
        gl.viewport(0, 0, w, h)
        return tex

    def run(self, cam, depth32, norm16, lights, plan, scale=1.0, light_bias=0.25, max_dist=8.0, thickness=0.35,
            denoise=2.0, world=None, stats=None, look=None):
        """One RenderAreaShadows frame. plan: dict(samples, steps, half, near_steps). Returns (L,H,W) float32 [0,1].

        world (P1-lite cube path): None = the screen path only (HEAD shaders ignore the world uniforms anyway), else
        {'atlas': float32 (H, 1632) D32F window depth, 'channels': [per light: None, or dict(row, near, far, r_map,
        contact, seed)]}. Uploads exactly what LLPipeline::renderAreaShadows uploads on every pass.

        stats: a dict to fill with GPU milliseconds of the draws (gather_ms, blur_ms, upsample_ms) via GL_TIME_ELAPSED queries.

        look: None = upload none of the soft-shadow LOOK uniforms (a program that was never given them: the identity), else a dict of any of
        strength (1), contrast (1), world_amount (1), reach (0, metres), screen_amount (1): uploaded exactly as renderAreaShadows does
        (area_look = 1 - strength, contrast - 1, 1 - world amount; area_screen_reach = reach, screen amount)."""
        gl = self.gl
        Wd, Hd = cam.W, cam.H
        half = bool(plan['half'])
        tw, th = ((Wd + 1) // 2, (Hd + 1) // 2) if half else (Wd, Hd)
        self._texture('depth', 1, 0x81A6, Wd, Hd, 0x1902, 0x1405, depth32)
        self._texture('norm', 2, 0x805B, Wd, Hd, 0x1908, 0x1403, norm16)
        count = len(lights)
        assert 1 <= count <= 4
        # view -> agent rotation M = R^T (R = the world -> view rotation). The shader takes the ROWS of M as three vec3: row j of M
        # is column j of R, i.e. row j of R^T.
        wrot = pack_view_rotation(cam.view)
        if world is not None:
            atlas = np.ascontiguousarray(world['atlas'], np.float32)
            self._texture('atlas', 4, 0x8CAC, atlas.shape[1], atlas.shape[0], 0x1902, 0x1406, atlas)
        else:
            self._texture('atlas', 4, 0x8CAC, 4, 4, 0x1902, 0x1406, np.ones((4, 4), np.float32))
        gl.active(0x84C1)
        arr = np.zeros((4, 4), np.float32)
        seeds = np.zeros(4, np.uint32)
        for i, l in enumerate(lights):
            v = cam.view @ np.array([*l.pos, 1.0])
            arr[i] = (v[0], v[1], v[2], l.radius)
            seeds[i] = l.seed
        # The blur radius is the plan's: the production policy of the tree under test (relight_3d_plan.py). The inline
        # formula is only the fallback for a caller that supplies no plan radius.
        radius = float(plan['radius']) if 'radius' in plan else float(np.clip(denoise * scale * (0.5 if half else 1.0), 0, 8))
        # pass 1: gather at trace resolution
        g = self._target('gather', tw, th)
        pr = self.gather
        self._begin(pr)
        self._mat(pr, b'projection_matrix', cam.proj)
        self._mat(pr, b'inv_proj', np.linalg.inv(cam.proj))
        gl.ui(gl.loc(pr, b'depthMap'), 1)
        gl.ui(gl.loc(pr, b'normalMap'), 2)
        gl.u4fv(gl.loc(pr, b'area_lights'), 4, arr.ctypes.data_as(C.POINTER(gl.F)))
        gl.u4uiv(gl.loc(pr, b'area_seeds'), 1, seeds.ctypes.data_as(C.POINTER(gl.U)))
        gl.ui(gl.loc(pr, b'area_first'), 0)
        gl.ui(gl.loc(pr, b'area_count'), count)
        gl.ui(gl.loc(pr, b'area_samples'), int(plan['samples']))
        gl.ui(gl.loc(pr, b'area_steps'), int(plan['steps']))
        gl.ui(gl.loc(pr, b'area_thin_objects'), int(plan['near_steps'] > 0))
        gl.ui(gl.loc(pr, b'area_near_steps'), int(plan['near_steps']))
        gl.uf[1](gl.loc(pr, b'area_light_bias'), light_bias)
        gl.uf[2](gl.loc(pr, b'area_trace_res'), float(tw), float(th))
        gl.uf[3](gl.loc(pr, b'area_params'), max_dist, thickness, scale * (0.5 if half else 1.0))
        wflags = np.zeros(4, np.float32)
        wcontact = np.zeros(4, np.float32)
        wfade = np.zeros(4, np.float32)   # area_contact_fade: the owning NATIVE slot's fade (0 = unmapped, the default every existing fixture keeps)
        winfo = np.zeros((4, 4), np.float32)
        wlight = np.zeros((4, 4), np.float32)
        wseed = np.zeros(4, np.uint32)
        if world is not None:
            for i, ch in enumerate(world['channels'][:count]):
                if ch is None:
                    continue
                # 'owned': False models a projector that keeps its native SL map (SL map + contact): not world-owned, contact fade set.
                wflags[i] = 1.0 if ch.get('owned', True) else 0.0
                wfade[i] = ch.get('contact_fade', 0.0)
                wcontact[i] = ch['contact']
                winfo[i] = (ch['row'], ch['near'], ch['r_map'], float(ch.get('detail_shadows', False)))
                wlight[i] = (arr[i, 0], arr[i, 1], arr[i, 2], ch['far'])
                wseed[i] = ch['seed']
        lk = None
        if look is not None:
            lk = dict(strength=1.0, contrast=1.0, world_amount=1.0, reach=0.0, screen_amount=1.0)
            lk.update(look)
            gl.uf[2](gl.loc(pr, b'area_screen_reach'), float(np.clip(lk['reach'], 0.0, 8.0)), float(np.clip(lk['screen_amount'], 0.0, 1.0)))
        gl.u4fv(gl.loc(pr, b'area_world'), 1, wflags.ctypes.data_as(C.POINTER(gl.F)))
        gl.u4fv(gl.loc(pr, b'area_contact_dist'), 1, wcontact.ctypes.data_as(C.POINTER(gl.F)))
        gl.u4fv(gl.loc(pr, b'area_contact_fade'), 1, wfade.ctypes.data_as(C.POINTER(gl.F)))
        gl.color_mask(1, 1, 1, 1)
        self._timed('gather_ms', stats, lambda: gl.draw(4, 0, 3))
        # pass 2/3: separable blur (only when radius > 0), then full-resolution bilateral upsample
        scratch = None

        def filt(src_key, dst_key, w, h, ax, ay):
            nonlocal scratch
            dst = self._target(dst_key, w, h)
            fr = self.filter
            self._begin(fr)
            self._mat(fr, b'inv_proj', np.linalg.inv(cam.proj))
            inverse_depth = np.linalg.inv(cam.proj)
            exact_depth = inverse_depth[[3, 3], [2, 3]]
            high_depth = exact_depth.astype(np.float32)
            low_depth = (exact_depth-high_depth.astype(np.float64)).astype(np.float32)
            gl.uf[4](gl.loc(fr, b'area_world_depth'), *map(float, np.r_[high_depth, low_depth]))
            gl.ui(gl.loc(fr, b'depthMap'), 1)
            gl.ui(gl.loc(fr, b'normalMap'), 2)
            gl.ui(gl.loc(fr, b'area_source'), 3)
            gl.ui(gl.loc(fr, b'area_debug_channel'), 0)
            gl.uf[2](gl.loc(fr, b'area_axis'), ax, ay)
            gl.uf[1](gl.loc(fr, b'area_radius'), radius)
            if lk is not None:
                vec = np.array([1.0 - np.clip(lk['strength'], 0.0, 1.0), np.clip(lk['contrast'], 0.25, 4.0) - 1.0,
                                1.0 - np.clip(lk['world_amount'], 0.0, 1.0)], np.float32)
                gl.u3fv(gl.loc(fr, b'area_look'), 1, vec.ctypes.data_as(C.POINTER(gl.F)))
            gl.u4fv(gl.loc(fr, b'area_world'), 1, wflags.ctypes.data_as(C.POINTER(gl.F)))
            gl.u4fv(gl.loc(fr, b'area_contact_fade'), 1, wfade.ctypes.data_as(C.POINTER(gl.F)))
            gl.u4fv(gl.loc(fr, b'area_world_light'), 4, wlight.ctypes.data_as(C.POINTER(gl.F)))
            gl.u4fv(gl.loc(fr, b'area_world_info'), 4, winfo.ctypes.data_as(C.POINTER(gl.F)))
            gl.u4uiv(gl.loc(fr, b'area_world_seed'), 1, wseed.ctypes.data_as(C.POINTER(gl.U)))
            gl.u3fv(gl.loc(fr, b'area_world_rot'), 3, wrot.ctypes.data_as(C.POINTER(gl.F)))
            gl.ui(gl.loc(fr, b'area_world_atlas'), 4)
            gl.active(0x84C3)
            gl.bind_tex(0x0DE1, self.textures[src_key][0])
            self._timed('upsample_ms' if (ax == 0.0 and ay == 0.0) else 'blur_ms', stats, lambda: gl.draw(4, 0, 3))

        # ping-pong: gather -> scratch(x) -> gather(y) -> visibility(full res)
        # the targets must not alias their own input, so bind distinct textures per pass
        def retarget(dst_key, w, h):
            return self._target(dst_key, w, h)
        src = 'gather'
        if radius > 0:
            filt('gather', 'scratch', tw, th, 1.0, 0.0)
            filt('scratch', 'gather2', tw, th, 0.0, 1.0)
            src = 'gather2'
        filt(src, 'vis', Wd, Hd, 0.0, 0.0)
        gl.finish()
        out = np.empty((Hd, Wd, 4), np.uint8)
        gl.read(0, 0, Wd, Hd, 0x1908, 0x1401, out.ctypes.data)
        return (out[:, :, :count].astype(np.float32) / 255.0).transpose(2, 0, 1).copy()
