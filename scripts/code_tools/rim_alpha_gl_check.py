"""Real-WGL regressions for subtle light wrap and alpha-hair occlusion.

Runs production wrap + blur shaders and rigRimTermEx, with straight-alpha
blending over an opaque red silhouette. Only scene/depth/normal/DT inputs are
fixtures. The production C++ call site determines whether wrap runs before or
after transparency. --shader-ref reads any committed version without edits.
No viewer build. Missing GL or an unrecognised call site is a FAILURE.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image

import relight_3d_gl as G


def check_state(runner, tree, out):
    """Execute the production C++ State transaction in this real WGL context.

    The DLL contains the exact production struct; its state test draws a
    clear to attachment zero and verifies sentinel MRT contents. The SSR test
    also extracts the production helper/call, with a small target wrapper over
    real color/depth FBOs. No viewer or LLRender cache is instantiated.
    """
    pipe = tree.read('indra/newview/pipeline.cpp')
    begin = pipe.index('struct State', pipe.index('void LLPipeline::renderRigRimWrap('))
    end = pipe.index('} saved(', begin) + 1
    state = pipe[begin:end] + ';'
    # Windows' GL 1.1 header plus the core functions used by the production
    # struct. Resolve them from the already-current WGL context at DLL load.
    constants = dict(GL_BLEND_SRC_RGB=0x80C9, GL_BLEND_DST_RGB=0x80C8,
                     GL_BLEND_SRC_ALPHA=0x80CB, GL_BLEND_DST_ALPHA=0x80CA,
                     GL_BLEND_EQUATION_RGB=0x8009, GL_BLEND_EQUATION_ALPHA=0x883D,
                     GL_DRAW_BUFFER0=0x8825, GL_COLOR_ATTACHMENT0=0x8CE0,
                     GL_FUNC_ADD=0x8006)
    api = dict(glIsEnabledi=('GLboolean', 'GLenum, GLuint'),
               glGetBooleani_v=('void', 'GLenum, GLuint, GLboolean*'),
               glGetIntegeri_v=('void', 'GLenum, GLuint, GLint*'),
               glEnablei=('void', 'GLenum, GLuint'), glDisablei=('void', 'GLenum, GLuint'),
               glColorMaski=('void', 'GLuint, GLboolean, GLboolean, GLboolean, GLboolean'),
               glBlendFuncSeparatei=('void', 'GLuint, GLenum, GLenum, GLenum, GLenum'),
               glBlendEquationSeparatei=('void', 'GLuint, GLenum, GLenum'),
               glDrawBuffers=('void', 'GLsizei, const GLenum*'),
               glBindFramebuffer=('void', 'GLenum, GLuint'),
               glBlitFramebuffer=('void', 'GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum'),
               glClearBufferfv=('void', 'GLenum, GLint, const GLfloat*'))
    declarations = '\n'.join('using P_%s=%s(APIENTRY*)(%s); auto %s=reinterpret_cast<P_%s>(wglGetProcAddress("%s"));' %
                             (name, ret, args, name, name, name) for name, (ret, args) in api.items())
    src = '#include <windows.h>\n#include <GL/gl.h>\n#include <array>\n#include <cassert>\nusing U32=unsigned;\n#define llassert assert\n'
    src += '\n'.join('#define %s %s' % kv for kv in constants.items()) + '\n' + declarations + '\n'
    src += state + '''
extern "C" __declspec(dllexport) void cycle(unsigned count) {
    State saved(count);
    glDisablei(GL_BLEND, 0);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &attachment);
    glBlendEquationSeparatei(0, GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparatei(0, GL_ONE, GL_ONE, GL_ONE, GL_ONE);
    const GLfloat value[4] = {.9f,.8f,.7f,0.f};
    glClearBufferfv(0x1800, 0, value);
}
extern "C" __declspec(dllexport) State* begin() { return new State(1); }
extern "C" __declspec(dllexport) void end(State* saved) { delete saved; }
'''
    # Execute the production SSR helper AND its actual finalize call. Targets
    # are tiny real color/depth FBOs, so source/depth selection is not merely a
    # Python rendering of a policy inferred from a setting or shader name.
    ssr = pipe[pipe.index('void LLPipeline::copyScreenSpaceReflections('):pipe.index('// Night Mask:')]
    declaration = ssr[:ssr.index('\n{')].replace('LLPipeline::', '') + ';'
    finalize = pipe[pipe.index('void LLPipeline::renderFinalize()'):]
    call = finalize[finalize.index('copyScreenSpaceReflections('):]
    call = call[:call.index(';')+1]
    src += '''
#define LL_PROFILE_GPU_ZONE(x)
struct LLGLDepthTest { LLGLDepthTest(GLboolean, GLboolean, GLenum) {} };
bool RenderScreenSpaceReflections = true, gCubeSnapshot = false;
struct LLRenderTarget {
    GLuint fbo; int width, height;
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void copyContents(LLRenderTarget& source, int sx0, int sy0, int sx1, int sy1,
                      int dx0, int dy0, int dx1, int dy1, unsigned mask, unsigned filter) {
        GLint read, draw;
        glGetIntegerv(0x8CAA, &read); glGetIntegerv(0x8CA6, &draw);
        glBindFramebuffer(0x8CA8, source.fbo); glBindFramebuffer(0x8CA9, fbo);
        glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
        glBindFramebuffer(0x8CA8, read); glBindFramebuffer(0x8CA9, draw);
    }
};
struct LLPipeline {
    struct Targets { LLRenderTarget screen, rigRimWrapScene; } mMainRT, *mRT = &mMainRT;
    LLRenderTarget mSceneMap;
    bool mRigRimWrapSceneValid;
''' + declaration + '\n};\n' + ssr + '''
extern "C" __declspec(dllexport) void capture(unsigned scene, unsigned prewrap, unsigned history,
                                            int w, int h, bool valid) {
    LLPipeline pipeline;
    pipeline.mMainRT = {{scene, w, h}, {prewrap, w, h}};
    pipeline.mSceneMap = {history, w, h};
    pipeline.mRigRimWrapSceneValid = valid;
    auto& mMainRT = pipeline.mMainRT;
    auto mRT = pipeline.mRT;
    auto& mSceneMap = pipeline.mSceneMap;
    auto mRigRimWrapSceneValid = pipeline.mRigRimWrapSceneValid;
    // Qualify only the method call; keep the production arguments verbatim.
    pipeline.''' + call + '\n}\n'
    source, dll = out / 'state.cpp', out / 'state.dll'
    source.write_text(src)
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    command = out / 'state.cmd'
    command.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /LD "%s" /Fe:"%s" /Fo:"%s" /link opengl32.lib /IMPLIB:"%s"\nexit /b %%ERRORLEVEL%%\n' %
                       (vcvars, source, dll, out / 'state.obj', out / 'state.lib'))
    compile_run = subprocess.run(['cmd', '/d', '/c', str(command)], capture_output=True, text=True)
    assert compile_run.returncode == 0, compile_run.stdout + compile_run.stderr
    native = C.CDLL(str(dll))
    native.cycle.argtypes = [C.c_uint]
    native.cycle.restype = None
    native.begin.restype = C.c_void_p
    native.end.argtypes = [C.c_void_p]
    native.end.restype = None
    native.capture.argtypes = [C.c_uint, C.c_uint, C.c_uint, C.c_int, C.c_int, C.c_bool]
    native.capture.restype = None
    runner.state_native = native
    gl = runner.gl
    getint = gl.fn('glGetIntegerv', None, gl.U, C.POINTER(gl.I))
    geti = gl.fn('glGetIntegeri_v', None, gl.U, gl.U, C.POINTER(gl.I))
    getb = gl.fn('glGetBooleani_v', None, gl.U, gl.U, C.POINTER(C.c_ubyte))
    is_enabled = gl.fn('glIsEnabledi', C.c_ubyte, gl.U, gl.U)
    mask = gl.fn('glColorMaski', None, gl.U, *([C.c_ubyte]*4))
    blend = gl.fn('glBlendFuncSeparatei', None, *([gl.U]*5))
    equation = gl.fn('glBlendEquationSeparatei', None, gl.U, gl.U, gl.U)
    enable, disable = [gl.fn(n, None, gl.U, gl.U) for n in ('glEnablei', 'glDisablei')]
    buffers = gl.fn('glDrawBuffers', None, gl.I, C.POINTER(gl.U))
    readbuffer = gl.fn('glReadBuffer', None, gl.U)
    sentinels = []
    for i in range(4):
        data = np.full((8, 8, 4), .1*(i+1), np.float32)
        tex = runner._texture('state%d' % i, 7, 0x8814, 8, 8, 0x1908, 0x1406, data)
        gl.bind_fbo(0x8D40, runner.fbo)
        gl.fbo_tex(0x8D40, 0x8CE0+i, 0x0DE1, tex, 0)
        sentinels.append(data)
    assert gl.fbo_status(0x8D40) == 0x8CD5
    gl.viewport(0, 0, 8, 8)
    def snapshot():
        answer = []
        for i in range(4):
            m = (C.c_ubyte*4)()
            getb(0x0C23, i, m)
            values = []
            for key in constants:
                if key.startswith('GL_BLEND_'):
                    value = gl.I()
                    geti(constants[key], i, C.byref(value))
                    values.append(value.value)
            route = gl.I()
            getint(0x8825+i, C.byref(route))
            read = gl.I()
            getint(0x0C02, C.byref(read))
            answer.append((tuple(m), is_enabled(0x0BE2, i), tuple(values), route.value, read.value))
        polygon = (gl.I*2)()
        getint(0x0B40, polygon)
        return answer, tuple(polygon)
    for iteration in range(4):
        gl.fn('glPolygonMode', None, gl.U, gl.U)(0x0408, (0x1B02, 0x1B01, 0x1B00, 0x1B01)[iteration])
        if iteration == 3:
            gl.fn('glPolygonMode', None, gl.U, gl.U)(0x0404, 0x1B00)
        for i in range(4):
            (enable if (i+iteration)%2 else disable)(0x0BE2, i)
            mask(i, 1, i%2, iteration%2, 1)
            blend(i, 0x0302, 0x0303, 1, 0)
            equation(i, 0x800A if i%2 else 0x8006, 0x800B)
        routes = (gl.U*4)(*[0x8CE0+i for i in reversed(range(4))])
        buffers(4, routes)
        readbuffer(0x8CE0 + iteration)
        before = snapshot()
        native.cycle(4)
        assert snapshot() == before, 'production C++ failed to restore GL state'
        for i in (1, 2, 3):
            readbuffer(0x8CE0+i)
            assert np.array_equal(runner.pixels(8, 8), sentinels[i]), 'auxiliary MRT overwritten'
        readbuffer(0x8CE0)
        assert np.all(runner.pixels(8, 8)[..., 3] == .1), 'glow alpha overwritten'
    # Detach auxiliary fixtures before subsequent render tests.
    for i in (1, 2, 3):
        gl.fbo_tex(0x8D40, 0x8CE0+i, 0x0DE1, 0, 0)
    route = gl.U(0x8CE0)
    buffers(1, C.byref(route))
    gl.color_mask(1, 1, 1, 1)
    equation(0, 0x8006, 0x8006)
    gl.disable(0x0BE2)
    gl.fn('glPolygonMode', None, gl.U, gl.U)(0x0408, 0x1B02)
    assert gl.fn('glGetError', gl.U)() == 0, 'GL error in production state transaction'
    print('PASS: exact production C++ state transaction, four MRTs, masks/blend/equations/routing/glow')


class Runner(G.AreaRunner):
    def __init__(self, tree):
        self.tree, self.gl, self.textures = tree, G.GL(), {}
        gl = self.gl
        self.get_error = gl.fn('glGetError', gl.U)
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0):
            gl.disable(cap)
        self.enable = gl.fn('glEnable', None, gl.U)
        self.blend = gl.fn('glBlendFuncSeparate', None, gl.U, gl.U, gl.U, gl.U)
        self.fbo, self.vbo = gl.U(), gl.U()
        gl.gen_fbo(1, C.byref(self.fbo))
        gl.gen_buf(1, C.byref(self.vbo))
        gl.bind_buf(0x8892, self.vbo)
        verts = np.array([-1, -1, 0, 3, -1, 0, -1, 3, 0], np.float32)
        gl.buffer_data(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
        self.wrap = self.link_pass('rigRimWrapF.glsl', common=True)
        self.down = self.link_pass('rigRimWrapBlurF.glsl', '#define RIG_RIM_WRAP_DOWNSAMPLE 1\n')
        self.blur = self.link_pass('rigRimWrapBlurF.glsl')
        util = tree.read(G.SHADERS + '/class1/deferred/deferredUtil.glsl')
        # Entire production rim implementation, not a CPU transcription.
        rim = util[util.index('uniform int  rig_rim_mode;'):util.index('// [RigRim] Forward-alpha punctual light WITH')]
        hair = rim + '''
in vec2 vary_fragcoord;
uniform sampler2D coverage;
out vec4 frag_color;
void main() {
    vec3 n = normalize(vec3(.98, 0, .2));
    vec3 c = vec3(.45);
    if (rigRimMode() != 0)
        c += rigRimTermEx(n, vec3(0,0,1), normalize(vec3(1,0,-1)),
            vec3(1), vec3(.7), .8, vec4(1,3,.35,0), rigRimForwardPixel(n,n,0));
    frag_color = vec4(c, texture(coverage, vary_fragcoord).r);
}
'''
        self.hair = self.link_pass(None, source=hair)
        # Exact production debug display shader and vertex path.
        pr = self.gl.program()
        for name, stage in (('copyV.glsl', 0x8B31), ('copyF.glsl', 0x8B30)):
            self.gl.attach(pr, self._compile(tree.read(G.SHADERS + '/class1/interface/' + name), name, stage))
        self.gl.link(pr)
        ok = self.gl.I()
        self.gl.getprog(pr, 0x8B82, C.byref(ok))
        assert ok.value, 'production copy program failed to link'
        self.copy = pr

    def link_pass(self, name, defines='', common=False, source=None):
        gl, pr = self.gl, self.gl.program()
        if common:
            for module in self.FRAGMENT_COMMON:
                path = self._production_file(module, 1 if module in self.LEVEL1 else 3)
                gl.attach(pr, self._compile(self.tree.read(path), module))
        if source is None:
            source = self.tree.read(G.SHADERS + '/class1/deferred/' + name)
        gl.attach(pr, self._compile(defines + source, name or 'production rim'))
        vertex = self.tree.read(G.SHADERS + '/class1/deferred/postDeferredNoTCV.glsl')
        gl.attach(pr, self._compile(vertex, 'screen vertex', 0x8B31))
        gl.link(pr)
        ok = gl.I()
        gl.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.plog(pr, len(log), None, log)
            raise AssertionError(log.value.decode())
        return pr

    def uniform(self, pr, name, *values):
        self.gl.uf[len(values)](self.gl.loc(pr, name.encode()), *values)

    def sampler(self, pr, name, tex, unit, linear=False):
        gl = self.gl
        gl.active(0x84C0 + unit)
        gl.bind_tex(0x0DE1, tex)
        for pname in (0x2800, 0x2801):
            gl.param(0x0DE1, pname, 0x2601 if linear else 0x2600)
        gl.ui(gl.loc(pr, name.encode()), unit)

    def target(self, tex, w, h):
        gl = self.gl
        gl.bind_fbo(0x8D40, self.fbo)
        gl.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, tex, 0)
        assert gl.fbo_status(0x8D40) == 0x8CD5
        gl.viewport(0, 0, w, h)

    def pixels(self, w, h):
        out = np.empty((h, w, 4), np.float32)
        self.gl.read(0, 0, w, h, 0x1908, 0x1406, out.ctypes.data)
        assert np.all(np.isfinite(out)), 'non-finite shader output'
        return out

    def capture_history(self, scene, snapshot, depth, coverage, w, h):
        gl = self.gl
        history = self._texture('history', 7, 0x8814, w, h, 0x1908, 0x1406, None)
        # Deliberately distinguish live post-alpha depth from the depthless
        # pre-wrap snapshot. Reusing that snapshot must not lose SSR depth.
        live = np.ascontiguousarray(np.where(coverage > .5, .35, depth), np.float32)
        dep = self._texture('live_depth', 7, 0x8CAC, w, h, 0x1902, 0x1406, live)
        out_depth = self._texture('history_depth', 7, 0x8CAC, w, h, 0x1902, 0x1406, None)
        if not hasattr(self, 'history_fbos'):
            self.history_fbos = (gl.U*3)()
            gl.gen_fbo(3, self.history_fbos)
        for fbo, color, d in zip(self.history_fbos, (scene, snapshot or scene, history), (dep, 0, out_depth)):
            gl.bind_fbo(0x8D40, fbo)
            gl.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, color, 0)
            gl.fbo_tex(0x8D40, 0x8D00, 0x0DE1, d, 0)
            assert gl.fbo_status(0x8D40) == 0x8CD5
        self.state_native.capture(*self.history_fbos, w, h, snapshot is not None)
        copied_depth = np.empty((h, w), np.float32)
        gl.read(0, 0, w, h, 0x1902, 0x1406, copied_depth.ctypes.data)
        self.history_depth_exact = np.array_equal(copied_depth, live)
        self.target(history, w, h)
        self.history = self.pixels(w, h)

    def frame(self, coverage, early, enabled=True, wrap_on=True, internal=0x881A,
              no_band=False, projection_y=1, amount=.25, master=.05,
              hdr_clamp=1, foreground=(.055, .009, .006), background=(2, 2, 2),
              review=None, debug=False, wireframe=False, reflection=None):
        gl = self.gl
        h, w = coverage.shape
        qw, qh = (w + 3) // 4, (h + 3) // 4
        body = np.zeros((h, w, 4), np.float32)
        body[:, :96] = [*foreground, .37]  # opaque body + glow tag
        body[:, 96:, :3] = background
        if reflection is not None:
            # A reflective background samples the prior history's subject edge.
            # The reflection hit coordinate is a fixture, like depth/normal/DT;
            # production wrap shading, snapshot and copy remain real GPU work.
            body[:, 96:, :3] += reflection[:, 94:95, :3]
        depth = np.full((h, w), .5, np.float32)
        depth[:, 96:] = 1
        aux = np.zeros_like(body)
        aux[:] = [.95, .5, 1, 1]  # grazing normal, valid actor
        yy, xx = np.mgrid[:h//2, :w//2]
        sil = np.zeros((h//2, w//2, 4), np.float32)
        sil[..., 0] = 47 - xx
        sil[..., 2] = 4
        sil[..., 3] = np.abs(47 - xx)
        if no_band:
            sil[..., 3] = 1e4
        def tex(key, a, fmt=0x1908, storage=0x8814):
            return self._texture(key, 7, storage, a.shape[1], a.shape[0], fmt, 0x1406, np.ascontiguousarray(a, np.float32))
        scene = tex('scene', body)
        cov = tex('coverage', coverage, 0x1903, 0x822E)
        dep = tex('depth', depth, 0x1903, 0x822E)
        normal = tex('aux', aux)
        seeds = tex('sil', sil)
        q = [self._texture('q%d' % i, 7, internal, qw, qh, 0x1908, 0x1406, None) for i in range(2)]
        gl.color_mask(1, 1, 1, 1)
        gl.disable(0x0BE2)
        snapshot, cached = None, None
        self.wrap_draws = self.blur_draws = 0
        self.polygon_restored = True
        polygon = gl.fn('glPolygonMode', None, gl.U, gl.U)
        def begin_pass():
            polygon(0x0408, 0x1B01 if wireframe else 0x1B02)
            return self.state_native.begin() if review is not None else None

        def end_pass(state):
            if state is not None:
                self.state_native.end(state)
            mode = (gl.I*2)()
            gl.fn('glGetIntegerv', None, gl.U, C.POINTER(gl.I))(0x0B40, mode)
            self.polygon_restored &= tuple(mode) == ((0x1B01,)*2 if wireframe else (0x1B02,)*2)
            # Hair geometry is a filled fixture; only the fullscreen passes are
            # under test here, not the intentionally different wireframe mesh.
            polygon(0x0408, 0x1B02)

        def hair():
            self.target(scene, w, h)
            self._begin(self.hair)
            self.sampler(self.hair, 'coverage', cov, 0)
            gl.ui(gl.loc(self.hair, b'rig_rim_mode'), int(enabled))
            self.uniform(self.hair, 'rig_rim_globals', .05, .5, .5, .25)
            self.uniform(self.hair, 'rig_rim_quality', .5, .5, 0, 0)
            self.enable(0x0BE2)
            # lldrawpoolalpha.cpp: SRC_ALPHA / ONE_MINUS_SRC_ALPHA,
            # glow suppression ZERO / ONE_MINUS_SRC_ALPHA.
            self.blend(0x0302, 0x0303, 0, 0x0303)
            gl.draw(4, 0, 3)
            gl.disable(0x0BE2)

        def wrap():
            nonlocal snapshot, cached
            if not enabled or not wrap_on or amount <= 0 or master <= 0:
                return
            state = begin_pass()
            # The production pass takes an independent full-res scene snapshot.
            # Copy actual rendered pixels (including hair in the late negative
            # control), rather than regenerating the foreground from fixtures.
            # Match scene storage exactly, as production does with RGBA16F.
            snapshot = self._texture('prewrap', 7, 0x8814, w, h, 0x1908, 0x1406, None)
            self.target(scene, w, h)
            gl.fn('glCopyTexSubImage2D', None, gl.U, gl.I, gl.I, gl.I, gl.I, gl.I, gl.I, gl.I)(
                0x0DE1, 0, 0, 0, 0, 0, w, h)
            for pr, src, dst, step in ((self.down, scene, q[0], (0, 0)),
                                      (self.blur, q[0], q[1], (.5/qw, 0)),
                                      (self.blur, q[1], q[0], (0, .5/qh))):
                self.target(dst, qw, qh)
                self._begin(pr)
                self.sampler(pr, 'rigRimWrapSrc', src, 0, True)
                self.uniform(pr, 'rig_rim_wrap2', *step, 0, 0)
                gl.draw(4, 0, 3)
                self.blur_draws += 1
            if debug:
                cached = self._texture('debug', 7, 0x881A, w, h, 0x1908, 0x1406, None)
            self.target(cached if debug else scene, w, h)
            self._begin(self.wrap)
            for unit, (name, t) in enumerate((('rigRimWrapSrc', q[0]), ('depthMap', dep),
                                             ('rigRimAuxMap', normal), ('rigRimSilMap', seeds),
                                             ('diffuseRect', snapshot))):
                self.sampler(self.wrap, name, t, unit, unit == 0)
            self.uniform(self.wrap, 'screen_res', w, h)
            self.uniform(self.wrap, 'rig_rim_sil', 4, .5, .08, 0)
            self.uniform(self.wrap, 'rig_rim_wrap', amount, 4, .5, hdr_clamp)
            self.uniform(self.wrap, 'rig_rim_wrap2', master, 0, int(debug), 1)
            inv = np.diag([1, 1/projection_y, 1, 1]).astype(np.float32)
            inv[2, 3] = -4  # fixture view depth 4 at window depth .5
            self._mat(self.wrap, b'inv_proj', inv)
            if not debug:
                self.enable(0x0BE2)
            self.blend(1, 1, 1, 1)
            gl.color_mask(1, 1, 1, 0)
            gl.draw(4, 0, 3)
            self.wrap_draws += 1
            gl.color_mask(1, 1, 1, 1)
            gl.disable(0x0BE2)
            end_pass(state)

        if debug and review is not None:
            if review['cached_debug']:
                wrap()
                hair()
            else:
                hair()
                wrap()
        elif early:
            wrap()
            hair()
        else:
            hair()
            wrap()
        if review is not None:
            # The existing post-alpha SSR capture selects the production source.
            self.capture_history(scene, snapshot, depth, coverage, w, h)
            if debug and cached is not None:
                self.target(cached, w, h)
                self.debug_term = self.pixels(w, h)
                self.target(scene, w, h)
                state = begin_pass()
                self._begin(self.copy)
                self.sampler(self.copy, 'diffuseMap', cached, 0)
                gl.disable(0x0BE2)
                gl.color_mask(1, 1, 1, 0)
                gl.draw(4, 0, 3)
                gl.color_mask(1, 1, 1, 1)
                end_pass(state)
        self.target(scene, w, h)
        result = self.pixels(w, h)
        assert self.get_error() == 0, 'GL error in production shader fixture'
        return result


def production_order(tree):
    pipe = tree.read('indra/newview/pipeline.cpp')
    geom = pipe[pipe.index('void LLPipeline::renderGeomPostDeferred('):pipe.index('void LLPipeline::renderGeomPostDeferred(') + 22000]
    finalize = pipe[pipe.index('void LLPipeline::renderFinalize()'):pipe.index('void LLPipeline::renderFinalize()') + 22000]
    if 'renderRigRimWrap(false);' in geom:
        call = geom.index('renderRigRimWrap(false);')
        assert geom.index('if (cur_type >= water_haze_pass && !done_roto_ink_scene)') < call
        assert call < geom.index('if (cur_type >= water_haze_pass && !done_water_haze)')
        assert call < geom.index('p->renderPostDeferred(i);')
        assert pipe.count('renderRigRimWrap(false);') == 1
        assert pipe.count('renderRigRimWrap(true);') == 1
        assert 'renderRigRimWrap(true);' in finalize
        assert 'gRigRimWrapBlurProgram[0].bind()' not in finalize
        helper = pipe[pipe.index('void LLPipeline::renderRigRimWrap('):pipe.index('void LLPipeline::renderFinalize()')]
        if 'mRigRimWrapDebugValid' in helper:
            assert 'debug_only && (debug_view() != 5 || !mRigRimWrapDebugValid)' in helper
            assert 'const F32 projection_y = gGLProjection[5];' in helper
            display = helper[helper.index('if (debug_only)'):helper.index('// One-sided lerp')]
            assert 'gCopyProgram.bind()' in display and 'rigRimWrapDebug' in display
            assert 'gRigRimWrapProgram.bind' not in display
            assert helper.count('gRigRimWrapBlurProgram[0].bind()') == 1
        else:
            assert '(debug_view() == 5) != debug_only' in helper
            assert 'debug_only ? gGLLastProjection[5] : gGLProjection[5]' in helper
        assert helper.index('!ALCineRigRim::isEnabled()') < helper.index('gGL.flush();')
        # No global mask/blend writes or nested indexed guard in this window.
        for forbidden in ('gGL.setColorMask(', 'gGL.blendFunc(', 'gGL.setSceneBlendType(',
                          'LLGLDisable blend', 'LLScopedIndexedDrawBufferGuard '):
            assert forbidden not in helper, forbidden
        assert 'glDrawBuffers(1, &attachment);' in helper
        assert 'if (!already_bound) mRT->screen.bindTarget();' in helper
        assert 'if (!already_bound) mRT->screen.flush();' in helper
        if tree.ref == 'WORKTREE':
            assert 'rigRimWrapScene.copyColorToAttachment0(mRT->screen,' in helper
            assert 'LLShaderMgr::DEFERRED_DIFFUSE, &mMainRT.rigRimWrapScene' in helper
            assert 'llclamp(finite_or(strength, .25f), 0.f, 1.f)' in helper
            assert 'llclamp(finite_or(master, 0.f), 0.f, 4.f)' in helper
        return True
    assert 'gRigRimWrapBlurProgram[0].bind()' in finalize, 'unknown production wrap scheduling'
    return False


def check_strength(runner, storage):
    """Production GPU draws: HDR/dark/tinted cases, full Amount/Master sweeps.

    The independent bounds are on the final composite, never the shader's
    internal k. This catches missing foreground subtraction or Master upload.
    """
    zero = np.zeros((96, 128), np.float32)
    weights = np.array([.2126, .7152, .0722])
    amounts = [0, .01, .05, .1, .25, .5, .75, 1]
    checks, measurements = {}, []
    subtle = runner.frame(zero, True, internal=storage, amount=.12, master=.6,
                          foreground=(.02, .02, .02), background=(2, 2, 2))
    subtle_source = runner.frame(zero, True, internal=storage, wrap_on=False,
                                 foreground=(.02, .02, .02), background=(2, 2, 2))
    subtle_gain = float(np.max((subtle[:, :96, :3] - subtle_source[:, :96, :3]) @ weights))
    checks['subtle_edge_preset_below_2_percent_of_capped_background'] = 0 < subtle_gain < .02
    for label, fg, bg in (('hdr2', (.02, .02, .02), (2, 2, 2)),
                          ('hdr8', (.02, .02, .02), (8, 8, 8)),
                          ('tinted', (1.5, .2, .01), (16, 4, 2)),
                          ('darker', (.2, .2, .2), (.01, .01, .01))):
        options = dict(internal=storage, foreground=fg, background=bg)
        source = runner.frame(zero, True, wrap_on=False, **options)
        for master in (0, .05, .25, 1, 4):
            frames = [runner.frame(zero, True, amount=a, master=master, **options) for a in amounts]
            prefix = '%s/master%g/' % (label, master)
            checks[prefix + 'zero_byte_identical'] = frames[0].tobytes() == source.tobytes()
            checks[prefix + 'monotonic'] = all(np.all(b[..., :3] >= a[..., :3]) for a, b in zip(frames, frames[1:]))
            checks[prefix + 'never_darker'] = all(np.all(a[..., :3] >= source[..., :3]) for a in frames)
            checks[prefix + 'glow_untouched'] = all(np.array_equal(a[..., 3], source[..., 3]) for a in frames)
            gain = frames[3][:, :96, :3] - source[:, :96, :3]
            peak = float(np.max(gain @ weights))
            checks[prefix + 'subtle_at_0.1'] = peak < .03 * float(np.dot(bg, weights))
            # Even at full Amount/high Master, approach the capped background
            # by at most 60%, per channel. Brighter source channels stay exact.
            upper = .6 * np.maximum(1 - source[..., :3], 0)
            checks[prefix + 'bounded_lerp'] = all(np.all(a[..., :3] - source[..., :3] <= upper + 1e-6) for a in frames)
            if label == 'darker' or master == 0:
                checks[prefix + 'no_light_negative_control'] = all(np.array_equal(a, source) for a in frames)
            elif label in ('hdr2', 'hdr8'):
                checks[prefix + 'positive_control'] = peak > 0
            if label in ('hdr2', 'hdr8'):
                measurements.append(dict(scene=label, master=master, amount_0_1_peak_gain=peak,
                                         background_fraction=peak / float(np.dot(bg, weights)),
                                         amount_0_25_peak_gain=float(np.max((frames[4][:, :96, :3] - source[:, :96, :3]) @ weights))))
        low = runner.frame(zero, True, amount=.5, master=.05, **options)
        high = runner.frame(zero, True, amount=.5, master=1, **options)
        checks[label + '/master_monotonic'] = bool(np.all(high[..., :3] >= low[..., :3]))
        if label in ('hdr2', 'hdr8'):
            checks[label + '/master_attenuates'] = bool(np.max(high[..., :3] - low[..., :3]) > .01)
        # Legacy clamp values must not restore an HDR glow; legacy amounts
        # over one saturate. These exercise the shader's defensive clamps too.
        full = runner.frame(zero, True, amount=1, master=1, **options)
        legacy = runner.frame(zero, True, amount=2, master=1, hdr_clamp=32, **options)
        checks[label + '/legacy_saturates'] = np.array_equal(full, legacy)
        off = runner.frame(zero, True, amount=1, wrap_on=False, **options)
        checks[label + '/wrap_off_byte_identical'] = off.tobytes() == source.tobytes()
    return dict(measurements=measurements, subtle_edge_peak_gain=subtle_gain, checks=checks)


def check_review(runner, tree, coverage, storage):
    """Three P2 regressions, driven by the production host schedule/state.

    Shader draws, history copies and cached-term display execute on WGL. Only
    reflection hit locations and scene geometry are fixtures. The State scope
    is compiled verbatim from the selected C++ revision by check_state().
    """
    pipe = tree.read('indra/newview/pipeline.cpp')
    helper = pipe[pipe.index('void LLPipeline::renderRigRimWrap('):pipe.index('void LLPipeline::renderFinalize()')]
    finalize = pipe[pipe.index('void LLPipeline::renderFinalize()'):pipe.index('void LLPipeline::renderFinalize()') + 22000]
    policy = dict(clean_history='mRigRimWrapSceneValid ? &mMainRT.rigRimWrapScene : nullptr' in finalize,
                  cached_debug='if (cache_debug) mMainRT.rigRimWrapDebug.bindTarget();' in helper)
    if policy['clean_history']:
        copy = pipe[pipe.index('void LLPipeline::copyScreenSpaceReflections('):pipe.index('// Night Mask:')]
        assert 'dst->copyContents(*color_src,' in copy
        assert 'color_src ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT' in copy
        assert helper.index('copyColorToAttachment0(mRT->screen,') < helper.index('mRigRimWrapSceneValid = true;')
        reset = pipe[pipe.index('if (source == ActorMaskViewSource::MAIN && mRT == &mMainRT)'):]
        reset = reset[:reset.index('\n    }')]
        assert 'mRigRimWrapSceneValid = false;' in reset
        assert 'mRigRimWrapDebugValid = false;' in reset
    else:
        assert 'copyScreenSpaceReflections(&mRT->screen, &mSceneMap);' in finalize
    if policy['cached_debug']:
        assert 'if (!cache_debug) glEnablei(GL_BLEND, 0);' in helper
        assert 'mRigRimWrapDebugValid = true;' in helper
        assert helper.index('if (debug_only)') < helper.index('gCopyProgram.bind()') < helper.index('// One-sided lerp')
        assert 'gRigRimWrapProgram' not in helper[helper.index('if (debug_only)'):helper.index('// One-sided lerp')]
    else:
        assert '(debug_view() == 5) != debug_only' in helper
    zero = np.zeros_like(coverage)
    checks, errors = {}, {}
    actual_history = expected_history = None
    for frame, wrap_on in enumerate((True, True, True, True, False, True, False)):
        # Camera cut: new projection, foreground and background, with the prior
        # history still present. Then toggle Wrap and Rim OFF on later frames.
        cut = frame >= 2
        options = dict(internal=storage, review=policy, amount=.75, master=1,
                       projection_y=1.7 if cut else 1,
                       foreground=(.03, .015, .01) if cut else (.02, .02, .02),
                       background=(.25, .6, .15) if cut else (.4, .5, .3),
                       enabled=frame != 6, wrap_on=wrap_on)
        actual = runner.frame(zero, True, reflection=actual_history, **options)
        actual_history = runner.history.copy()
        checks['SSR/post_alpha_depth_frame%d' % frame] = runner.history_depth_exact
        options['wrap_on'] = False
        expected = runner.frame(zero, True, reflection=expected_history, **options)
        expected_history = runner.history.copy()
        error = float(np.max(np.abs(actual_history - expected_history)))
        label = ('successive_0', 'successive_1', 'camera_cut', 'after_cut',
                 'wrap_toggle_off_next_frame', 'wrap_on_again', 'rim_toggle_off_next_frame')[frame]
        checks['SSR/' + label] = error == 0
        errors['SSR/' + label] = error
        if frame in (4, 6):
            checks['SSR/' + label + '_beauty'] = actual.tobytes() == expected.tobytes()
        if frame == 0:
            checks['SSR/beauty_positive_control'] = bool(np.max(actual[..., :3] - expected[..., :3]) > .001)

    options = dict(internal=storage, review=policy, amount=.75, master=1)
    debug = runner.frame(coverage, True, debug=True, **options)
    term = runner.debug_term.copy()
    checks['debug/display_exact_cached_term'] = np.array_equal(debug[..., :3], term[..., :3])
    checks['debug/single_preparation'] = runner.wrap_draws == 1 and runner.blur_draws == 3
    bare_debug = runner.frame(zero, True, debug=True, **options)
    error = float(np.max(np.abs(debug[..., :3] - bare_debug[..., :3])))
    checks['debug/pre_alpha_term_independent_of_hair'] = error == 0
    errors['debug/pre_alpha_term_independent_of_hair'] = error
    beauty = runner.frame(coverage, True, **options)
    beauty_history = runner.history.copy()
    checks['SSR/post_alpha_depth_with_hair'] = runner.history_depth_exact
    off = runner.frame(coverage, True, wrap_on=False, **options)
    checks['off/wrap_history_byte_identical_to_scene'] = runner.history.tobytes() == off.tobytes()
    rim_off = runner.frame(coverage, True, enabled=False, **options)
    checks['off/rim_history_byte_identical_to_scene'] = runner.history.tobytes() == rim_off.tobytes()
    runner.frame(zero, True, wrap_on=False, **options)
    checks['SSR/pre_alpha_color_when_wrap_active'] = np.array_equal(beauty_history, runner.history)
    solid = runner.frame(zero, True, **options)
    line = runner.frame(zero, True, wireframe=True, **options)
    checks['wireframe/filled_fullscreen_matches_fill'] = np.array_equal(solid, line)
    checks['wireframe/restores_polygon_mode'] = runner.polygon_restored
    errors['wireframe/filled_fullscreen_matches_fill'] = float(np.max(np.abs(solid - line)))
    wire_debug = runner.frame(coverage, True, debug=True, wireframe=True, **options)
    checks['wireframe/debug_matches_fill'] = np.array_equal(wire_debug[..., :3], debug[..., :3])
    checks['wireframe/debug_restores_polygon_mode'] = runner.polygon_restored
    # Keep this value live so a broken fixture cannot pass by drawing nothing.
    checks['debug/positive_control'] = bool(np.max(debug[..., :3]) > .001 and np.max(beauty[..., :3] - off[..., :3]) > .001)
    return dict(policy=policy, checks=checks, errors=errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shader-ref', default='WORKTREE')
    parser.add_argument('--out', type=Path, default=G.REPO / '.tmp/rim_fringe/gl')
    args = parser.parse_args()
    tree = G.SourceTree(args.shader_ref)
    early = production_order(tree)
    runner = Runner(tree)
    print('GL', runner.gl.renderer, flush=True)
    args.out.mkdir(parents=True, exist_ok=True)
    args.out = args.out.resolve()
    if early:
        check_state(runner, tree, args.out)
    coverage = np.zeros((96, 128), np.float32)
    # Thin strands, authored alpha ramp 0..1; ends and fully transparent gaps.
    for y in range(12, 84):
        tip = min(1, (y-12)/12, (83-y)/12)
        for centre in (83, 88, 93):
            coverage[y, centre-1:centre+2] = np.array([.15, 1, .35]) * tip
    zero = np.zeros_like(coverage)
    rows, failures = [], []
    for storage, name in ((0x8C3A, 'R11G11B10F'), (0x881A, 'RGBA16F')):
        actual = runner.frame(coverage, early, internal=storage)
        expected = runner.frame(coverage, True, internal=storage)
        bare = runner.frame(zero, True, internal=storage)
        hair = runner.frame(coverage, True, wrap_on=False, internal=storage)
        hair_bare = runner.frame(zero, True, wrap_on=False, internal=storage)
        late = runner.frame(coverage, False, internal=storage)
        transparent = (coverage == 0) & (np.indices(coverage.shape)[1] < 96)
        opaque = coverage == 1
        leak = float(np.max(np.abs(actual[transparent, :3] - bare[transparent, :3])))
        over = float(np.max(np.abs(actual[opaque, :3] - hair[opaque, :3])))
        error = float(np.max(np.abs(actual - expected)))
        off_a = runner.frame(coverage, False, enabled=False, internal=storage)
        off_b = runner.frame(coverage, True, enabled=False, internal=storage)
        smooth = runner.frame(coverage, early, no_band=True, internal=storage)
        repeated = runner.frame(coverage, early, internal=storage)
        moved = np.roll(coverage, -7, axis=1)
        # Two projections and a moving foreground, using fresh current inputs.
        runner.frame(moved, early, projection_y=1.7, internal=storage)
        returned = runner.frame(coverage, early, internal=storage)
        checks = dict(no_hair_feedback=leak < 1e-6, coverage_occludes_wrap=over < 1e-6,
                      correct_composite=error < 1e-6, rim_off_byte_identical=off_a.tobytes() == off_b.tobytes(),
                      local_rim_no_spill=np.array_equal(hair[transparent], hair_bare[transparent]),
                      hair_rim_retained=bool(np.max(hair[opaque, :3] - off_a[opaque, :3]) > .001),
                      glow_preserved=bool(np.array_equal(actual[..., 3], expected[..., 3])),
                      opaque_only_unchanged=np.array_equal(bare, runner.frame(zero, False, internal=storage)),
                      no_band_negative_control=np.array_equal(smooth, hair),
                      no_frame_history=np.array_equal(actual, repeated) and np.array_equal(actual, returned))
        checks['wrap_positive_control'] = bool(np.max(bare[..., :3] - hair_bare[..., :3]) > .001)
        checks['late_order_negative_control'] = bool(np.max(np.abs(late - expected)) > .001)
        # The local rim is weighted exactly once by authored coverage.
        full = np.ones_like(coverage)
        delta = runner.frame(full, True, wrap_on=False, internal=storage) - runner.frame(full, True, enabled=False, internal=storage)
        alpha_error = 0.
        for alpha in np.linspace(0, 1, 17):
            cov = full * alpha
            lit = runner.frame(cov, True, wrap_on=False, internal=storage)
            unlit = runner.frame(cov, True, enabled=False, internal=storage)
            alpha_error = max(alpha_error, float(np.max(np.abs((lit-unlit)[..., :3] - alpha*delta[..., :3]))))
        checks['rim_proportional_to_coverage'] = alpha_error < 1e-6
        strength = check_strength(runner, storage)
        review = check_review(runner, tree, coverage, storage) if early else None
        rows.append(dict(storage=name, transparent_leak=leak, opaque_overlay=over,
                         max_error=error, alpha_proportional_error=alpha_error, checks=checks,
                         strength=strength, review=review))
        failures += [name + ': ' + k for k, passed in checks.items() if not passed]
        failures += [name + ': ' + k for k, passed in strength['checks'].items() if not passed]
        if review:
            failures += [name + ': ' + k for k, passed in review['checks'].items() if not passed]
        for label, pixels in (('actual', actual), ('expected', expected)):
            srgb = np.clip(pixels[..., :3], 0, 1) ** (1/2.2)
            Image.fromarray(np.uint8(srgb * 255 + .5)).resize((512, 384)).save(args.out / (name + '_' + label + '.png'))
        # Diagnostic exposure, explicitly labelled: reveals the pale extra band
        # without changing the production rendering or its normal-exposure PNG.
        diff = np.clip(np.abs(actual[..., :3] - expected[..., :3]) * 100, 0, 1)
        Image.fromarray(np.uint8(diff * 255 + .5)).resize((512, 384)).save(args.out / (name + '_error_x100.png'))
    result = dict(ref=args.shader_ref, order='before alpha' if early else 'after alpha', rows=rows, failures=failures)
    (args.out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    assert not failures, 'FAIL: ' + '; '.join(failures)
    print('PASS: production wrap order, alpha 0..1, retained hair rim, glow, exact rim-off bytes')


if __name__ == '__main__':
    main()
