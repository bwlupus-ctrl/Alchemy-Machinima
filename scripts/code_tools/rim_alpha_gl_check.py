"""Real-WGL regressions for subtle light wrap and alpha-hair occlusion.

Runs production wrap + blur shaders and rigRimTermEx, with straight-alpha
blending over an opaque red silhouette. Only scene/depth/normal/DT inputs are
fixtures. The production C++ call site determines whether wrap runs before or
after transparency. --shader-ref HEAD reproduces the old order without edits.
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

    The DLL contains the exact production struct; the surrounding draw is a
    clear to attachment zero. Verify GL state AND sentinel MRT contents. No
    viewer objects or global LLRender cache are mocked or instantiated here.
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
'''
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
        return answer
    for iteration in range(4):
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

    def frame(self, coverage, early, enabled=True, wrap_on=True, internal=0x881A,
              no_band=False, projection_y=1, amount=.25, master=.05,
              hdr_clamp=1, foreground=(.055, .009, .006), background=(2, 2, 2)):
        gl = self.gl
        h, w = coverage.shape
        qw, qh = (w + 3) // 4, (h + 3) // 4
        body = np.zeros((h, w, 4), np.float32)
        body[:, :96] = [*foreground, .37]  # opaque body + glow tag
        body[:, 96:, :3] = background
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
            if not enabled or not wrap_on or amount <= 0 or master <= 0:
                return
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
            self.target(scene, w, h)
            self._begin(self.wrap)
            for unit, (name, t) in enumerate((('rigRimWrapSrc', q[0]), ('depthMap', dep),
                                             ('rigRimAuxMap', normal), ('rigRimSilMap', seeds),
                                             ('diffuseRect', snapshot))):
                self.sampler(self.wrap, name, t, unit, unit == 0)
            self.uniform(self.wrap, 'screen_res', w, h)
            self.uniform(self.wrap, 'rig_rim_sil', 4, .5, .08, 0)
            self.uniform(self.wrap, 'rig_rim_wrap', amount, 4, .5, hdr_clamp)
            self.uniform(self.wrap, 'rig_rim_wrap2', master, 0, 0, 1)
            inv = np.diag([1, 1/projection_y, 1, 1]).astype(np.float32)
            inv[2, 3] = -4  # fixture view depth 4 at window depth .5
            self._mat(self.wrap, b'inv_proj', inv)
            self.enable(0x0BE2)
            self.blend(1, 1, 1, 1)
            gl.color_mask(1, 1, 1, 0)
            gl.draw(4, 0, 3)
            gl.color_mask(1, 1, 1, 1)
            gl.disable(0x0BE2)

        if early:
            wrap()
            hair()
        else:
            hair()
            wrap()
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
        rows.append(dict(storage=name, transparent_leak=leak, opaque_overlay=over,
                         max_error=error, alpha_proportional_error=alpha_error, checks=checks,
                         strength=strength))
        failures += [name + ': ' + k for k, passed in checks.items() if not passed]
        failures += [name + ': ' + k for k, passed in strength['checks'].items() if not passed]
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
