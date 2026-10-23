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

    def __init__(self, tree):
        self.tree = tree
        self.gl = GL()
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

    def _link(self, file):
        gl = self.gl
        name = Path(file).name
        fragment = list(self.FRAGMENT_COMMON) + ['deferred/screenSpaceRayUtil.glsl']
        vertices = ['deferred/areaShadowV.glsl', 'deferred/textureUtilV.glsl', 'objects/nonindexedTextureV.glsl']
        pr = gl.program()
        for module in fragment:
            path = self._production_file(module, 1 if module in self.LEVEL1 else 3)
            gl.attach(pr, self._compile(self.tree.read(path), name + ' ' + module))
        gl.attach(pr, self._compile(self.tree.read(SHADERS + '/' + file), name))
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
            denoise=2.0):
        """One RenderAreaShadows frame. plan: dict(samples, steps, half, near_steps). Returns (L,H,W) float32 [0,1]."""
        gl = self.gl
        Wd, Hd = cam.W, cam.H
        half = bool(plan['half'])
        tw, th = ((Wd + 1) // 2, (Hd + 1) // 2) if half else (Wd, Hd)
        self._texture('depth', 1, 0x81A6, Wd, Hd, 0x1902, 0x1405, depth32)
        self._texture('norm', 2, 0x805B, Wd, Hd, 0x1908, 0x1403, norm16)
        count = len(lights)
        assert 1 <= count <= 4
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
        gl.color_mask(1, 1, 1, 1)
        gl.draw(4, 0, 3)
        # pass 2/3: separable blur (only when radius > 0), then full-resolution bilateral upsample
        scratch = None

        def filt(src_key, dst_key, w, h, ax, ay):
            nonlocal scratch
            dst = self._target(dst_key, w, h)
            fr = self.filter
            self._begin(fr)
            self._mat(fr, b'inv_proj', np.linalg.inv(cam.proj))
            gl.ui(gl.loc(fr, b'depthMap'), 1)
            gl.ui(gl.loc(fr, b'normalMap'), 2)
            gl.ui(gl.loc(fr, b'area_source'), 3)
            gl.ui(gl.loc(fr, b'area_debug_channel'), 0)
            gl.uf[2](gl.loc(fr, b'area_axis'), ax, ay)
            gl.uf[1](gl.loc(fr, b'area_radius'), radius)
            gl.active(0x84C3)
            gl.bind_tex(0x0DE1, self.textures[src_key][0])
            gl.draw(4, 0, 3)

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
