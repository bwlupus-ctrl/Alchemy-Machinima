#!/usr/bin/env python3
"""Clone transitions, RENDER layer gate (workstream R, Phase 1; PLAN.md rev 3 sections 3.1-3.4, 7).

Run with PYTHONUTF8=1 from the worktree root:  python scripts/code_tools/clone_transition_render_check.py [--selftest]
No viewer build. Real WGL (relight_3d_gl's context) and the PRODUCTION GLSL modules; the C++ side is the pure
header indra/newview/alclonetransition.h / alclonetransitionrender.h compiled into a native fixture (cl).

Proves, with numbers:
  (a) field parity    C++ fieldValue()/edgeWeight() vs the GLSL port, <= 1e-5, all three styles
  (b) off identity    style 0 renders byte-identically to the pre-change shaders (dissolve module, silCoverF)
  (c) complement      each sample is kept by exactly ONE half, p in {0,.25,.5,.75,1}, all styles
  (d) entry agreement beauty entry (actorFxTronPrime) == shadow/velocity entry (actorFxDissolveDiscard)
                      == silhouette cover (silCoverF); precedence = transition OR existing coverage
  (e) shell           additive edge only: pixels outside the edge band are bit-identical, band pixels only brighten
  (g) coordinate      packEyeToBody() reproduces bodyFromAgent() through an arbitrary view matrix
  (s) structure       every touched vertex shader writes the coordinate; one resolver; no actorFxEnabled==3; shell
                      never primes depth / never reuses look 10 / never adds matte coverage; Crossfade is object-space
  (f) PLANTED NEGATIVES that must FAIL: seed differs between halves; shell with a depth prime; effect_only
                      replaced by look 10; screen Bayer used for entity Crossfade in the shadow program.
"""
import ctypes as C
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

os.environ.setdefault('PYTHONUTF8', '1')
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/code_tools'))
SH = ROOT / 'indra/newview/app_settings/shaders'
NV = ROOT / 'indra/newview'
BASE = 'a49168443e7'
FAILS = []


def check(cond, label):
    print(('PASS  ' if cond else 'FAIL  ') + label)
    if not cond:
        FAILS.append(label)
    return cond


def text(path):
    return Path(path).read_bytes().decode('utf-8').replace('\r\n', '\n')


def base_text(rel):
    return subprocess.check_output(['git', 'show', BASE + ':' + rel], cwd=ROOT).decode('utf-8').replace('\r\n', '\n')


# ---------------------------------------------------------------------------------------------------------
# (s) structure
# ---------------------------------------------------------------------------------------------------------
def vertex_files():
    return sorted(p for p in SH.rglob('*V.glsl') if 'out vec3 vary_actor_fx_position' in text(p))


def structure_checks():
    bad = []
    files = vertex_files()
    for p in files:
        s = text(p)
        ok = ('out vec3 vary_transition_pos;' in s and 'uniform mat4 transitionEyeToBody;' in s
              and 'uniform vec4 actorFxTransition;' in s and 'vary_transition_pos =' in s
              and 'actorFxTransition.x > 0.5' in s)
        if not ok:
            bad.append(p.name)
    check(not bad and len(files) >= 43, 'every vertex shader that writes vary_actor_fx_position also writes vary_transition_pos (%d files) %s' % (len(files), bad))
    for name in ('actorghostV.glsl', 'avatarActorGhostV.glsl', 'eyeballActorGhostV.glsl', 'actorMaskV.glsl',
                 'actorMaskAvatarV.glsl', 'actorMaskEyesV.glsl'):
        s = text(next(SH.rglob(name)))
        check('out vec3 vary_transition_pos;' in s and 'vary_transition_pos =' in s, 'transition coordinate in ' + name)
    dis = text(SH / 'class1/alchemy/actorFxDissolveF.glsl')
    pri = text(SH / 'class1/alchemy/actorFxF.glsl')
    fb = text(SH / 'class1/alchemy/actorFxDissolveFallbackF.glsl')
    check('bool actorFxTransitionDiscard()' in dis and 'bool actorFxTransitionDiscard()' in fb,
          'dissolve module and identity fallback both define actorFxTransitionDiscard')
    prime = pri[pri.index('void actorFxTronPrime()'):]
    prime = prime[:prime.index('\n}\n')]
    check('actorFxTransitionDiscard()' in prime and 'discard;' in prime,
          'beauty entry actorFxTronPrime discards through the transition mask')
    dd = dis[dis.index('bool actorFxDissolveDiscard(vec3 object_position)'):]
    dd = dd[:dd.index('\n}\n')]
    check(dd.index('actorFxTransitionDiscard()') < dd.index('actorFxDissolveEnabled()'),
          'shadow/velocity entry tests the transition BEFORE any treatment early-out')
    tr = dis[dis.index('bool actorFxTransitionDiscard()'):]
    tr = tr[:tr.index('\n}\n')]
    check('actorFxEnabled' not in tr and 'actorFxLook' not in tr, 'transition mask is independent of actorFxEnabled / look (no ==3 reuse)')
    check('actorFxEnabled == 3' not in dis and 'actorFxEnabled==3' not in dis, 'no actorFxEnabled == 3 coverage hack')
    # object-space Crossfade: the field must not read the screen
    field = field_block(dis)
    check('gl_FragCoord' not in field and 'bayer' not in field.lower() and 'cloneFadeCoverage' not in field,
          'Crossfade (and every field) is object-space: no screen Bayer / gl_FragCoord')
    # the FIELD block is one text in every consumer
    blocks = {n: field_block(text(next(SH.rglob(n)))) for n in ('actorFxDissolveF.glsl', 'silCoverF.glsl', 'actorghostF.glsl')}
    check(len(set(blocks.values())) == 1, 'GLSL field block is byte-identical in dissolve module, silCoverF and actorghostF')
    # upload side
    dp = text(NV / 'lldrawpool.cpp')
    check('transitionCoverageFor(primary, fallback)' in dp and dp.count('upload_native_clone_transition(') >= 3,
          'ONE resolver (transitionCoverageFor) feeds both lookup paths')
    stylefn = dp[dp.index('bool LLRenderPass::uploadActorFxStyleOnly('):]
    stylefn = stylefn[:stylefn.index('\n}\n')]
    check(stylefn.index('upload_native_clone_transition(') > stylefn.index('shared_actor_fx_replay_active(actor_id)')
          and 'return false;' not in stylefn.split('upload_native_clone_transition(')[0].split('shared_actor_fx_replay_active')[1],
          'UUID path uploads the transition even when replay disabled the treatment (no early return before it)')
    check('sCloneTransitionDirty.empty()' in dp, 'idle path: no GL work when no program holds a mask')
    dc = text(NV / 'lldirectorcast.cpp')
    tc = dc[dc.index('LLDirectorCast::transitionCoverageFor('):]
    tc = tc[:tc.index('\n}\n')]
    check('!ALCloneTransitions::anyRunning()' in tc and 'return inactive' in tc, 'resolver early-outs when nothing runs')
    mover = text(NV / 'llactormover.cpp')
    check(shell_branch_ok(mover)[0], 'effect shell: ' + shell_branch_ok(mover)[1])
    check(shell_dispatch_ok(mover)[0], 'effect shell dispatch: ' + shell_dispatch_ok(mover)[1])
    check(shell_no_matte_ok(mover)[0], 'effect shell never adds matte coverage / depth prepass / replay-active queries: ' + shell_no_matte_ok(mover)[1])
    check('uploadCloneTransition(*sh, gp.mTransition)' in mover and mover.count('uploadCloneTransition(') >= 5,
          'Studio overlay, replay, silhouette cover and system-avatar programs all receive the transition uniforms')
    pipe = text(NV / 'pipeline.cpp')
    check('transitionCoverageFor(di.mActorFxOwner, di.mActorFxFallbackOwner)' in pipe, 'silhouette cover batches get the native transition mask')
    check('layered' in mover and 'studioLayersFor(inst.mId, layers)' in mover, 'Studio overlay draws one pass per transition layer (Look->Look dual draw)')
    check('item.mEntity' in mover and 'gp.mScale = 1.f;' in mover and 'gp.mRotation = LLQuaternion();' in mover,
          'a1 entity branch uses ATTACHED placement (identity rotation, scale 1)')
    prm = text(NV / 'lldrawpool.cpp')
    check('coverageChanged(' in prm and 'transition active' in prm, 'actor mask coverage-change check knows "transition active"')


def field_block(src):
    a = src.index('// BEGIN CLONE TRANSITION FIELD')
    b = src.index('// END CLONE TRANSITION FIELD')
    return src[a:b]


def shell_branch_ok(mover):
    """The effect-only branch of drawGeometryGhost: additive, no depth write, LEQUAL, never a prime."""
    i = mover.index('else if (gp.mEffectOnly)')
    j = mover.index('\n    else\n    {\n', i)
    body = mover[i:j]
    if 'gp.mNativeDepthAvailable' not in body:
        return False, 'does not require native depth (would have to prime)'
    if re.search(r'LLGLDepthTest\s+\w+\(\s*GL_TRUE\s*,\s*GL_TRUE', body):
        return False, 'writes depth (prime / depth write)'
    if 'GL_LESS' in body:
        return False, 'uses GL_LESS (a prime-style depth test)'
    if 'BF_SOURCE_ALPHA, LLRender::BF_ONE' not in body:
        return False, 'not additive (SRC_ALPHA, ONE)'
    if 'setColorMask(false' in body:
        return False, 'colour-less draw = depth prime'
    return True, 'additive SRC_ALPHA/ONE, depth test only (LEQUAL), requires native depth, no prime'


def shell_dispatch_ok(mover):
    i = mover.index('if (proxy.mEffectShell)\n    {\n        // [CloneTransition] effect shell')
    j = mover.index('if (!proxy.mActive || !proxy.mAvatar || proxy.mAvatar->isDead() ||\n        (ALDerenderList', i)
    body = mover[i:j]
    if 'mEffectOnly = true' not in body:
        return False, 'effect_only flag not set'
    if 'GHOST_STYLE_DISSOLVE' in body or re.search(r'\b10\b', body):
        return False, 'replaces effect_only with look 10 (Layer + look 10 owns native beauty)'
    if 'GHOST_STYLE_CLONE' not in body:
        return False, 'shell not drawn through the textured clone look'
    if 'mSuppressNativeBeauty' in body:
        return False, 'sets native-beauty suppression'
    return True, 'explicit mEffectOnly flag, textured clone look, no look 10, no suppression'


def shell_no_matte_ok(mover):
    need = ['proxy.mActive && !proxy.mEffectShell && matches', 'proxy.mActive && !proxy.mEffectShell && proxy.mPBRReady',
            'proxy.mSuppressNativeBeauty && !proxy.mEffectShell && matches', '!proxy.mEffectShell && renderSharedActorStyleProxyItem(proxy, true)',
            'proxy.mEffectShell || !proxy.mAvatar']
    miss = [n for n in need if n not in mover]
    return (not miss), 'ok' if not miss else 'missing ' + str(miss)


# ---------------------------------------------------------------------------------------------------------
# C++ reference fixture
# ---------------------------------------------------------------------------------------------------------
FIXTURE = r'''
#include "alclonetransition.h"
#include "alclonetransitionrender.h"
#include <cstdio>
#include <cstdint>
#include <cmath>
using namespace ALCloneTransition;
int main()
{
    char tag;
    while (std::scanf(" %c", &tag) == 1)
    {
        if (tag == 'F')
        {
            int style, dir; unsigned seed; float h, x, y, z, p;
            std::scanf("%d %u %f %d %f %f %f %f", &style, &seed, &h, &dir, &x, &y, &z, &p);
            RenderParams r; r.active = true; r.style = style; r.seed = seed; r.bodyHeight = h; r.direction = dir; r.p = p;
            const float pos[3] = {x, y, z};
            std::printf("%.9g %.9g\n", fieldValue(r, pos), edgeWeight(r, fieldValue(r, pos)));
        }
        else if (tag == 'M')
        {
            // eyeToBody(view) applied to view*agent must equal bodyFromAgent(agent)
            RenderParams r; float v[16], ax, ay, az;
            std::scanf("%f %f %f %f", &r.frame.rootAgent[0], &r.frame.rootAgent[1], &r.frame.rootAgent[2], &r.frame.heading);
            std::scanf("%f", &r.frame.scale);
            for (int i = 0; i < 16; ++i) std::scanf("%f", &v[i]);
            std::scanf("%f %f %f", &ax, &ay, &az);
            float m[16];
            if (!ALCloneTransitionRender::packEyeToBody(r, v, m)) { std::printf("SINGULAR\n"); continue; }
            const float eye[4] = {v[0]*ax + v[4]*ay + v[8]*az + v[12], v[1]*ax + v[5]*ay + v[9]*az + v[13],
                                  v[2]*ax + v[6]*ay + v[10]*az + v[14], 1.f};
            float got[3];
            for (int row = 0; row < 3; ++row) got[row] = m[row]*eye[0] + m[4+row]*eye[1] + m[8+row]*eye[2] + m[12+row]*eye[3];
            const float agent[3] = {ax, ay, az}; float want[3];
            ALCloneTransitionRender::bodyFromAgent(r.frame, agent, want);
            std::printf("%.9g\n", std::fmax(std::fabs(got[0]-want[0]), std::fmax(std::fabs(got[1]-want[1]), std::fabs(got[2]-want[2]))));
        }
    }
    return 0;
}
'''


def build_fixture(directory):
    vcvars = None
    for root in (Path(os.environ.get('ProgramFiles', 'C:/Program Files')), Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))):
        found = sorted((root / 'Microsoft Visual Studio').glob('*/*/VC/Auxiliary/Build/vcvars64.bat'))
        if found:
            vcvars = found[-1]
            break
    assert vcvars, 'no MSVC toolchain (vcvars64.bat) found'
    src = directory / 'ct_fixture.cpp'
    src.write_text(FIXTURE, encoding='utf-8')
    cmd = directory / 'build.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul\ncl /nologo /std:c++17 /EHsc /fp:strict /I"%s" /Fo"%s" /Fe"%s" "%s"\nexit /b %%ERRORLEVEL%%\n'
                   % (vcvars, NV, directory / 'ct.obj', directory / 'ct.exe', src), encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(cmd)], check=True, capture_output=True)
    return directory / 'ct.exe'


def run_fixture(exe, lines):
    out = subprocess.run([str(exe)], input='\n'.join(lines) + '\n', text=True, capture_output=True, check=True).stdout.split('\n')
    return [l for l in out if l]


# ---------------------------------------------------------------------------------------------------------
# GL harness
# ---------------------------------------------------------------------------------------------------------
class Harness:
    VS = '''
uniform vec3 gridLo; uniform vec3 gridHi; uniform float gridY;
out vec3 vary_transition_pos; out vec3 vary_actor_fx_position; out vec3 vary_position; out vec2 vary_texcoord0;
void main(){
    int id = gl_VertexID; vec2 p = vec2((id << 1) & 2, id & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    vec3 pos = vec3(mix(gridLo.x, gridHi.x, p.x), gridY, mix(gridLo.z, gridHi.z, p.y));
    vary_transition_pos = pos; vary_actor_fx_position = pos; vary_position = pos; vary_texcoord0 = p;
}'''

    def __init__(self):
        import numpy as np
        import relight_3d_gl as G
        self.np, self.G = np, G
        self.gl = gl = G.GL()
        self.bindout = gl.fn('glBindFragDataLocation', None, C.c_uint, C.c_uint, C.c_char_p)
        self.err = gl.fn('glGetError', C.c_uint)
        self.blendfunc = gl.fn('glBlendFuncSeparate', None, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
        self.enable = gl.fn('glEnable', None, C.c_uint)
        self.clearcolor = gl.fn('glClearColor', None, *([C.c_float] * 4))
        self.clear = gl.fn('glClear', None, C.c_uint)
        self.drawbuffers = gl.fn('glDrawBuffers', None, C.c_int, C.POINTER(C.c_uint))
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44):
            gl.disable(cap)
        self.size = 64
        self.fbo = C.c_uint()
        gl.gen_fbo(1, C.byref(self.fbo))
        gl.bind_fbo(0x8D40, self.fbo)
        self.tex = C.c_uint()
        gl.gen_tex(1, C.byref(self.tex))
        gl.bind_tex(0x0DE1, self.tex)
        for p, v in ((0x2801, 0x2600), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F)):
            gl.param(0x0DE1, p, v)
        gl.teximage(0x0DE1, 0, 0x8814, self.size, self.size, 0, 0x1908, 0x1406, None)
        gl.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, self.tex, 0)
        assert gl.fbo_status(0x8D40) == 0x8CD5
        bufs = (C.c_uint * 1)(0x8CE0)
        self.drawbuffers(1, bufs)
        gl.viewport(0, 0, self.size, self.size)
        # a sampler texture for silCoverF
        self.white = C.c_uint()
        gl.gen_tex(1, C.byref(self.white))
        gl.bind_tex(0x0DE1, self.white)
        for p, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
            gl.param(0x0DE1, p, v)
        white = np.ones(4, np.float32)
        gl.teximage(0x0DE1, 0, 0x8814, 1, 1, 0, 0x1908, 0x1406, white.ctypes.data)
        self.cache = {}

    def shader(self, src, stage, defs='', label=''):
        gl = self.gl
        sh = gl.create_shader(stage)
        injection = self.G.PREAMBLE + defs + '\n'
        s = src.replace('/*[EXTRA_CODE_HERE]*/', injection, 1) if '/*[EXTRA_CODE_HERE]*/' in src else injection + src
        b = C.c_char_p(('#version 420\n' + s).encode())
        gl.source(sh, 1, C.byref(b), None)
        gl.compile(sh)
        ok = C.c_int()
        gl.getshader(sh, 0x8B81, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.slog(sh, len(log), None, log)
            raise RuntimeError('%s: %s' % (label, log.value.decode()))
        return sh

    def program(self, fragment_sources, vs=None, defs='', label=''):
        key = (label, tuple(hash(s) for s in fragment_sources), hash(vs or self.VS), defs)
        if key in self.cache:
            return self.cache[key]
        gl = self.gl
        pr = gl.program()
        gl.attach(pr, self.shader(vs or self.VS, 0x8B31, defs, label + ':vs'))
        for i, s in enumerate(fragment_sources):
            gl.attach(pr, self.shader(s, 0x8B30, defs, '%s:fs%d' % (label, i)))
        self.bindout(pr, 0, b'frag_color')
        gl.link(pr)
        ok = C.c_int()
        gl.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.plog(pr, len(log), None, log)
            raise RuntimeError('link %s: %s' % (label, log.value.decode()))
        self.cache[key] = pr
        return pr

    def set(self, pr, name, *vals):
        loc = self.gl.loc(pr, name.encode())
        if loc < 0:
            return
        if len(vals) == 1 and isinstance(vals[0], int):
            self.gl.ui(loc, vals[0])
        else:
            self.gl.uf[len(vals)](loc, *[float(v) for v in vals])

    def draw(self, pr, uniforms, lo=(-0.4, 0, 0.0), hi=(0.4, 0, 1.8), y=0.123, clear=(0, 0, 0, 0), blend=False):
        gl, np = self.gl, self.np
        gl.use(pr)
        self.set(pr, 'gridLo', *lo)
        self.set(pr, 'gridHi', *hi)
        self.set(pr, 'gridY', y)
        for k, v in uniforms.items():
            self.set(pr, k, *(v if isinstance(v, tuple) else (v,)))
        gl.active(0x84C0)
        gl.bind_tex(0x0DE1, self.white)
        self.set(pr, 'diffuseMap', 0)
        self.clearcolor(*clear)
        self.clear(0x4000)
        if blend:
            self.enable(0x0BE2)
            self.blendfunc(0x0302, 1, 0, 1)   # the viewer's shell state: SRC_ALPHA, ONE ; alpha (bloom) untouched
        else:
            gl.disable(0x0BE2)
        gl.draw(4, 0, 3)
        pix = np.empty(self.size * self.size * 4, np.float32)
        gl.read(0, 0, self.size, self.size, 0x1908, 0x1406, pix.ctypes.data)
        assert self.err() == 0, 'GL error'
        return pix.reshape(self.size, self.size, 4)


MASK_FS = '''
out vec4 frag_color;
bool actorFxTransitionDiscard();
void main(){ if (actorFxTransitionDiscard()) discard; frag_color = vec4(1.0, 0.0, 0.0, 1.0); }
'''
SHADOW_FS = '''
out vec4 frag_color;
in vec3 vary_actor_fx_position;
bool actorFxDissolveDiscard(vec3 object_position);
void main(){ if (actorFxDissolveDiscard(vary_actor_fx_position)) discard; frag_color = vec4(1.0, 0.0, 0.0, 1.0); }
'''
FIELD_FS = '''
out vec4 frag_color;
in vec3 vary_transition_pos;
uniform vec4 actorFxTransition; uniform vec4 actorFxTransition2;
uint ctSeed();
float ctField(int style, vec3 pos, uint seed, float body_height, int direction);
void main(){ frag_color = vec4(ctField(int(actorFxTransition.x + 0.5), vary_transition_pos, ctSeed(), actorFxTransition2.x, int(actorFxTransition.w + 0.5)), 0.0, 0.0, 1.0); }
'''
MIRROR_STUB = 'void mirrorClip(vec3 pos) {}\n'


def seed_halves(seed):
    return float(seed & 0xFFFF), float(seed >> 16)


def tuniforms(style, p, side, direction=0, height=1.8, edge=0.06, seed=12345):
    lo, hi = seed_halves(seed)
    return {'actorFxTransition': (style, p, side, direction), 'actorFxTransition2': (height, edge, lo, hi)}


def kept(img):
    return img[:, :, 0] > 0.5


def complement(h, dissolve_src, style, p, seed_a=777, seed_b=777):
    """Both halves rendered through the PRODUCTION mask; True when every sample is kept by exactly one half."""
    np = h.np
    pr = h.program([dissolve_src, MASK_FS], label='mask')
    ok = True
    for y in (0.07, 0.31, -0.2):
        a = kept(h.draw(pr, tuniforms(style, p, 0, seed=seed_a), y=y))
        b = kept(h.draw(pr, tuniforms(style, p, 1, seed=seed_b), y=y))
        ok &= bool(np.all(a ^ b)) and not bool(np.any(a & b))
    return ok


def gl_checks(exe):
    np = __import__('numpy')
    h = Harness()
    dis = text(SH / 'class1/alchemy/actorFxDissolveF.glsl')
    sil = text(SH / 'class1/deferred/silCoverF.glsl')
    pri = text(SH / 'class1/alchemy/actorFxF.glsl')
    check(h.err() == 0, 'WGL context: ' + str(h.gl.renderer() if callable(getattr(h.gl, 'renderer', None)) else 'ok'))

    # ---- compile every touched vertex shader (compile-only, both permutations) ----
    skin_stub = '''mat4 getObjectSkinnedTransform(){ return mat4(1.0); }
mat4 getSkinnedTransform(){ return mat4(1.0); }
mat4 getLastObjectSkinnedTransform(){ return mat4(1.0); }
mat4 getLastSkinnedTransform(){ return mat4(1.0); }
'''
    compiled = 0
    failures = []
    ghostF = text(SH / 'class1/interface/actorghostF.glsl')
    for gdefs in ('', '#define GHOST_WORLD_PASS 1\n', '#define GHOST_WORLD_PASS 1\n#define GHOST_SHARED_DISSOLVE 1\n', '#define AL_KEY_FILL 1\n'):
        try:
            h.shader(ghostF + '\n', 0x8B30, gdefs + '#define MAX_JOINTS_PER_MESH_OBJECT 110\n', 'actorghostF')
            compiled += 1
        except RuntimeError as e:
            failures.append(('actorghostF.glsl', str(e).splitlines()[0][:160]))
    for p in vertex_files() + [SH / 'class1/deferred/actorMaskV.glsl', SH / 'class1/interface/actorghostV.glsl',
                               SH / 'class1/avatar/avatarActorGhostV.glsl', SH / 'class1/avatar/eyeballActorGhostV.glsl']:
        s = text(p)
        for defs in ('#define HAS_ACTOR_FX 1\n', '#define HAS_ACTOR_FX 1\n#define HAS_SKIN 1\n'):
            if 'getSkinnedTransform' not in s and 'getObjectSkinnedTransform' not in s and 'HAS_SKIN' not in defs and False:
                continue
            try:
                h.shader(s + '\n', 0x8B31, defs + '#define MAX_JOINTS_PER_MESH_OBJECT 110\n#define GLTF_INDEXED_CHANNELS 16\n#define SHARED_ACTOR_FX_ALPHA_MODE 1\n', p.name)
                compiled += 1
            except RuntimeError as e:
                msg = str(e)
                # genuine module externals (skin / atmospherics / material helpers) are link-time, not compile errors
                failures.append((p.name, msg.splitlines()[0][:160]))
    check(not failures, 'all touched vertex shaders compile in both HAS_SKIN permutations (%d compiles) %s' % (compiled, failures[:3]))

    # ---- (a) field parity ----
    import random
    rnd = random.Random(20261116)

    def safe_pos():
        while True:
            x, y, z = rnd.uniform(-0.6, 0.6), rnd.uniform(-0.4, 0.4), rnd.uniform(-0.2, 2.0)
            if all(min(abs((c / cell) % 1.0), abs(1.0 - (c / cell) % 1.0)) > 2e-3 for c in (x, y, z) for cell in (0.11, 0.037, 0.015)):
                return x, y, z
    probes, lines = [], []
    fprog = h.program([dis, FIELD_FS], label='field')
    for style in (1, 2, 3):
        for _ in range(160):
            pos = safe_pos()
            seed = rnd.randrange(1, 2 ** 32)
            direction = rnd.randrange(2)
            height = rnd.choice((1.5, 1.8, 2.4))
            p = rnd.random()
            probes.append((style, pos, seed, direction, height, p))
            lines.append('F %d %u %.9g %d %.9g %.9g %.9g %.9g' % (style, seed, height, direction, pos[0], pos[1], pos[2], p))
    ref = run_fixture(exe, lines)
    worst = 0.0
    for (style, pos, seed, direction, height, p), line in zip(probes, ref):
        want = float(line.split()[0])
        u = tuniforms(style, p, 0, direction, height, seed=seed)
        img = h.draw(fprog, u, lo=pos, hi=pos, y=pos[1])
        # constant position everywhere: constrain gridLo.z/hi.z; gridY carries y
        got = float(img[h.size // 2, h.size // 2, 0])
        worst = max(worst, abs(got - want))
    check(worst <= 1e-5, 'field parity C++ vs GLSL: %d probes x 3 styles, worst |delta| = %.3g (<= 1e-5)' % (len(probes), worst))
    # edge weight parity (the shell term), extracted from the PRODUCTION actorghostF
    gh = text(SH / 'class1/interface/actorghostF.glsl')
    ew = gh[gh.index('float ctEdgeWeight('):]
    ew = ew[:ew.index('\n}\n') + 3]
    edge_fs = 'out vec4 frag_color;\nin vec3 vary_transition_pos;\nuniform vec4 actorFxTransition; uniform vec4 actorFxTransition2;\n' \
              'uint ctSeed();\nfloat ctField(int style, vec3 pos, uint seed, float body_height, int direction);\n' + ew + \
              'void main(){ float f = ctField(int(actorFxTransition.x + 0.5), vary_transition_pos, ctSeed(), actorFxTransition2.x, int(actorFxTransition.w + 0.5));\n' \
              ' frag_color = vec4(ctEdgeWeight(int(actorFxTransition.x + 0.5), f, actorFxTransition.y, actorFxTransition2.y), f, 0.0, 1.0); }\n'
    eprog = h.program([dis, edge_fs], label='edge')
    worst = 0.0
    for (style, pos, seed, direction, height, p), line in zip(probes, ref):
        want = float(line.split()[1])
        img = h.draw(eprog, tuniforms(style, p, 0, direction, height, edge=0.06, seed=seed), lo=pos, hi=pos, y=pos[1])
        worst = max(worst, abs(float(img[h.size // 2, h.size // 2, 0]) - want))
    check(worst <= 1e-5, 'shell edge weight parity C++ edgeWeight() vs GLSL ctEdgeWeight: worst %.3g' % worst)

    # ---- (c) complement ----
    ok_all = True
    for style in (1, 2, 3):
        for p in (0.0, 0.25, 0.5, 0.75, 1.0):
            ok_all &= complement(h, dis, style, p)
    check(ok_all, 'complement: every sample kept by exactly one half (styles 1-3, p in {0,.25,.5,.75,1}, 3 slices)')
    # coverage actually moves: at p = .5 each half keeps a non-trivial share
    pr = h.program([dis, MASK_FS], label='mask')
    shares = [float(kept(h.draw(pr, tuniforms(s, 0.5, 0))).mean()) for s in (1, 2, 3)]
    check(all(0.15 < v < 0.85 for v in shares), 'p = .5 keeps a real share of the outgoing body for every style %s' % [round(v, 2) for v in shares])
    check(not complement(h, dis, 1, 0.5, seed_a=777, seed_b=778), 'PLANTED NEGATIVE: seed differing between halves is detected (holes/overlaps)')

    # ---- (d) entry agreement + precedence ----
    shadow = h.program([dis, SHADOW_FS], label='shadow')
    beauty_fs = '''out vec4 frag_color;
in vec3 vary_actor_fx_position;
void actorFxTronPrime();
void main(){ actorFxTronPrime(); frag_color = vec4(1.0, 0.0, 0.0, 1.0); }
'''
    try:
        beauty = h.program([dis, pri, beauty_fs], label='beauty', vs=None)
    except RuntimeError as e:
        beauty = None
        check(False, 'beauty entry program (actorFxF + dissolve module) links: ' + str(e)[:200])
    sil_fs = sil
    silprog = h.program([MIRROR_STUB + 'void unused_stub(){}', sil_fs], defs='#define SIL_COVER_SYSTEM 1\n#define MASK_CHANNELS 1\n', label='sil')
    agree = True
    for style in (1, 2, 3):
        for p in (0.0, 0.3, 0.5, 0.8, 1.0):
            for side in (0, 1):
                u = tuniforms(style, p, side)
                m0 = kept(h.draw(pr, u))
                m1 = kept(h.draw(shadow, dict(u, actorFxEnabled=0)))
                agree &= bool(np.array_equal(m0, m1))
                if beauty is not None:
                    m2 = kept(h.draw(beauty, dict(u, actorFxEnabled=0)))
                    agree &= bool(np.array_equal(m0, m2))
                su = dict(u, sil_cover=(1.0, 0.0), sil_blended=0, minimum_alpha=0.0)
                m3 = kept(h.draw(silprog, su))
                agree &= bool(np.array_equal(m0, m3))
    check(agree, 'beauty entry == shadow/velocity entry == silhouette cover on the same samples (styles 1-3, 5 p values, both sides)')
    # precedence: transition OR existing coverage (clone-fade full progress discards everything)
    union = kept(h.draw(shadow, dict(tuniforms(1, 0.5, 0), actorFxEnabled=2, actorFxDissolveProgress=1.0)))
    check(not union.any(), 'precedence: existing coverage (clone fade) still discards on top of the transition')
    solo = kept(h.draw(shadow, dict(tuniforms(1, 0.5, 0), actorFxEnabled=2, actorFxDissolveProgress=0.0)))
    check(np.array_equal(solo, kept(h.draw(pr, tuniforms(1, 0.5, 0)))), 'precedence: zero existing coverage change leaves the transition mask exactly')

    # ---- (b) transition-off identity vs the pre-change shaders ----
    dis0 = base_text('indra/newview/app_settings/shaders/class1/alchemy/actorFxDissolveF.glsl')
    sil0 = base_text('indra/newview/app_settings/shaders/class1/deferred/silCoverF.glsl')
    shadow_fs_old = SHADOW_FS
    old_shadow = h.program([dis0, shadow_fs_old], vs=Harness.VS, label='shadow_old')
    same = True
    for enabled, look, prog in ((0, 0, 0.5), (1, 10, 0.4), (1, 10, 0.9), (2, 0, 0.5), (2, 0, 0.0), (2, 0, 1.0)):
        for tphase in (0.0, 1.3):
            u = dict(actorFxEnabled=enabled, actorFxLook=look, actorFxDissolveProgress=prog, actorFxTime=tphase,
                     actorFxParams0=(1, 1, 0, 0), actorFxParams1=(0, 0, 1, 0.7))
            new_img = h.draw(shadow, dict(u, actorFxTransition=(0, 0, 0, 0)))
            old_img = h.draw(old_shadow, u)
            same &= new_img.tobytes() == old_img.tobytes()
    check(same, 'transition OFF: dissolve module renders byte-identically to the pre-change shader (clone fade + look 10 + off)')
    sil_old = h.program([MIRROR_STUB, sil0], defs='#define SIL_COVER_SYSTEM 1\n#define MASK_CHANNELS 1\n', label='sil_old')
    su = dict(sil_cover=(1.0, 0.5), sil_blended=0, minimum_alpha=0.0, sil_clone_progress=0.3)
    same = h.draw(silprog, dict(su, actorFxTransition=(0, 0, 0, 0))).tobytes() == h.draw(sil_old, su).tobytes()
    check(same, 'transition OFF: silhouette cover renders byte-identically to the pre-change shader')

    # ---- (s2) Crossfade must not follow the screen: same body points at two screen positions ----
    cf = h.program([dis, MASK_FS], label='mask')
    a = kept(h.draw(cf, tuniforms(3, 0.5, 0), lo=(-0.4, 0, 0.0), hi=(0.4, 0, 1.8)))
    # shift the whole screen footprint by drawing the same body points through a different viewport origin
    h.gl.viewport(5, 3, h.size - 8, h.size - 6)
    b2 = kept(h.draw(cf, tuniforms(3, 0.5, 0), lo=(-0.4, 0, 0.0), hi=(0.4, 0, 1.8)))
    h.gl.viewport(0, 0, h.size, h.size)
    check(object_space_stable(h, dis), 'Crossfade mask depends on the object-space coordinate only (screen shift leaves the kept set per sample unchanged)')
    bayer_dis = dis.replace('float ctField(int style, vec3 pos, uint seed, float body_height, int direction)\n{',
                            'float ctField(int style, vec3 pos, uint seed, float body_height, int direction)\n{\n    if (style == 3) { ivec2 q = ivec2(mod(floor(gl_FragCoord.xy), 4.0)); const float bayer[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.); return (bayer[q.y*4+q.x]+0.5)/16.0; }', 1)
    assert bayer_dis != dis
    stable = object_space_stable(h, bayer_dis)
    check(not stable, 'PLANTED NEGATIVE: screen Bayer for entity Crossfade in the shadow program is detected (mask moves with the screen)')

    # ---- (e) shell: additive only ----
    shell_fs = 'out vec4 frag_color;\nin vec3 vary_transition_pos;\nuniform vec4 actorFxTransition; uniform vec4 actorFxTransition2; uniform vec4 actorFxTransitionEdge;\n' \
               'uint ctSeed();\nfloat ctField(int style, vec3 pos, uint seed, float body_height, int direction);\nbool ctKeeps(int side, float field, float p);\n' + ew + \
               'void main(){ float f = ctField(int(actorFxTransition.x + 0.5), vary_transition_pos, ctSeed(), actorFxTransition2.x, int(actorFxTransition.w + 0.5));\n' \
               ' if (!ctKeeps(int(actorFxTransition.z + 0.5), f, actorFxTransition.y)) discard;\n' \
               ' float w = ctEdgeWeight(int(actorFxTransition.x + 0.5), f, actorFxTransition.y, actorFxTransition2.y);\n' \
               ' frag_color = vec4(actorFxTransitionEdge.rgb * (actorFxTransitionEdge.w * w), 1.0); }\n'
    sprog = h.program([dis, shell_fs], label='shell')
    bg = (0.21, 0.37, 0.52, 0.4)
    for style in (1, 2):
        u = dict(tuniforms(style, 0.5, 0), actorFxTransitionEdge=(1.0, 0.55, 0.15, 2.0))
        weight = h.draw(eprog, tuniforms(style, 0.5, 0, edge=0.06))[:, :, 0]
        keptmask = kept(h.draw(pr, tuniforms(style, 0.5, 0)))
        out = h.draw(sprog, u, clear=bg, blend=True)
        expect_bg = np.array(bg, np.float32)
        outside = (weight == 0) | ~keptmask
        identical = bool(np.all(out[outside] == expect_bg))
        band = (weight > 0) & keptmask
        brighter = bool(np.all(out[band][:, :3] >= expect_bg[:3] - 0.0)) and bool(band.any())
        check(identical and brighter, 'shell style %d: %d pixels outside the edge band bit-identical, %d band pixels only brighten' % (style, int(outside.sum()), int(band.sum())))
    # ---- (g) coordinate contract ----
    mlines = []
    for _ in range(60):
        r = rnd
        view = [r.uniform(-1, 1) for _ in range(16)]
        # a proper rigid camera view: rotation about z/x and translation
        import math
        a, b = r.uniform(0, 6.28), r.uniform(-1.2, 1.2)
        ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
        rz = [[ca, -sa, 0], [sa, ca, 0], [0, 0, 1]]
        rx = [[1, 0, 0], [0, cb, -sb], [0, sb, cb]]
        rot = [[sum(rx[i][k] * rz[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
        t = [r.uniform(-50, 50) for _ in range(3)]
        view = [rot[0][0], rot[1][0], rot[2][0], 0, rot[0][1], rot[1][1], rot[2][1], 0, rot[0][2], rot[1][2], rot[2][2], 0, t[0], t[1], t[2], 1]
        mlines.append('M %.6g %.6g %.6g %.6g %.6g %s %.6g %.6g %.6g' % (r.uniform(-90, 90), r.uniform(-90, 90), r.uniform(20, 60), r.uniform(-3, 3), r.uniform(0.5, 2.0),
                                                                          ' '.join('%.9g' % v for v in view), r.uniform(-90, 90), r.uniform(-90, 90), r.uniform(20, 62)))
    errs = [float(x) for x in run_fixture(exe, mlines) if x != 'SINGULAR']
    check(len(errs) == 60 and max(errs) < 2e-3, 'coordinate contract: packEyeToBody == bodyFromAgent through arbitrary views (max err %.3g m at body scale)' % max(errs))


def object_space_stable(h, dissolve_src):
    """Kept set per OBJECT-SPACE sample must not change when the same samples land on different pixels."""
    np = h.np
    pr = h.program([dissolve_src, MASK_FS], label='mask_cf')
    a = kept(h.draw(pr, tuniforms(3, 0.5, 0)))
    # same grid, but the fragment lands at a different screen location: scroll the body range by exactly
    # one screen pixel column (a Bayer 4x4 changes under a 1-pixel shift) and compare the overlapping columns
    px = 0.8 / h.size
    b = kept(h.draw(pr, tuniforms(3, 0.5, 0), lo=(-0.4 + px, 0, 0.0), hi=(0.4 + px, 0, 1.8)))
    # body sample at column c in `a` is at column c - 1 in `b`
    return bool(np.array_equal(a[:, 1:], b[:, :-1]))


def native_negatives():
    mover = text(NV / 'llactormover.cpp')
    # shell with a depth prime must FAIL the shell check
    planted = mover.replace('LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);\n                LLGLEnable blend(GL_BLEND);\n                gGL.setColorMask(true, true);\n                gGL.blendFunc(LLRender::BF_SOURCE_ALPHA, LLRender::BF_ONE,',
                            'LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_LESS);\n                LLGLEnable blend(GL_BLEND);\n                gGL.setColorMask(true, true);\n                gGL.blendFunc(LLRender::BF_SOURCE_ALPHA, LLRender::BF_ONE,', 1)
    assert planted != mover
    check(not shell_branch_ok(planted)[0], 'PLANTED NEGATIVE: shell with a depth prime / depth write is detected')
    planted = mover.replace('GHOST_STYLE_CLONE, shell_params, &proxy.mStaticFaces, &geometry_complete);',
                            'GHOST_STYLE_DISSOLVE, shell_params, &proxy.mStaticFaces, &geometry_complete);', 1)
    assert planted != mover
    check(not shell_dispatch_ok(planted)[0], 'PLANTED NEGATIVE: effect_only replaced by look 10 (Dissolve) is detected')
    planted = mover.replace('shell_params.mEffectOnly = true;', 'shell_params.mEffectOnly = false;', 1)
    check(not shell_dispatch_ok(planted)[0], 'PLANTED NEGATIVE: missing explicit effect_only flag is detected')
    planted = mover.replace('proxy.mSuppressNativeBeauty && !proxy.mEffectShell && matches', 'proxy.mSuppressNativeBeauty && matches', 1)
    check(not shell_no_matte_ok(planted)[0], 'PLANTED NEGATIVE: shell visible to the native-suppression query is detected')


def main():
    selftest = '--selftest' in sys.argv
    structure_checks()
    native_negatives()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            exe = build_fixture(Path(tmp))
            gl_checks(exe)
    except Exception as e:  # missing GL or toolchain is a failure, not a skip
        import traceback
        traceback.print_exc()
        check(False, 'GL/native fixture run: %s' % e)
    print()
    if FAILS:
        print('clone_transition_render_check: FAIL (%d)' % len(FAILS))
        for f in FAILS:
            print('   -', f)
        sys.exit(1)
    print('clone_transition_render_check: PASS')


if __name__ == '__main__':
    main()
