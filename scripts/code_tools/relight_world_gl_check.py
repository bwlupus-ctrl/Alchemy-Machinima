"""Mandatory real-GPU regression gate for the P1-lite world shadow path (Windows/WGL).

python scripts/code_tools/relight_world_gl_check.py [--glm-include DIR] [--out DIR] [--images DIR]

No CPU shadow atlas: production shadowV/F + Actor FX module rasterize six 544^2
D32F faces; glCopyImageSubData populates the production atlas. The production
gather/filter/resolve consume it. Host rotation bytes come from the compiled C++
production packing helper with the viewer's aligned GLM definitions. G-buffer
receivers alone are analytic. Missing GL, compiler, or GLM is a FAILURE, never SKIP.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_cube_source as CS
import relight_3d_plan as PL
import relight_3d_tasks as T
import relight_3d_reference as R
import relight_3d_metrics as M
import relight_world_math_check as WM

REPO = G.REPO


def lifecycle_checks():
    """Viewer-only integration contracts supplement the executed C++ unit scopes.

    The standalone context cannot instantiate avatars or LLPipeline. Pin the
    actual event/call sites too, so the tested helpers cannot become disconnected.
    """
    pipe = (REPO / 'indra/newview/pipeline.cpp').read_text()
    def body(start, end): return pipe[pipe.index(start):pipe.index(end,pipe.index(start))]
    cube = body('void LLPipeline::generateWorldShadowCubes(', 'void LLPipeline::renderGroups(')
    residency = body('if (cube_main && !gSnapshot)', 'take.worldOn = cube_main;')
    release = body('void LLPipeline::releaseWorldShadowTargets(bool clear_failure)', '\nnamespace')
    # Tie the executed host helper and GL copy to the viewer call sites. A
    # correct helper left unused by a reverted pipeline must fail this gate too.
    assert 'ALWorldShadow::packViewRotation(view,world_rot)' in pipe and 'std::memcpy(world_rot' not in pipe
    extents = body('static bool world_shadow_extents(', 'static F32 worldShadowLightNearUncached(')
    assert 'drawable->isActive()' in extents and 'ALWorldShadow::transformedAabb(drawable->getRenderMatrix().mMatrix' in extents
    assert pipe.count('worldShadowLightNear(') == 3, 'raster and resolve must both use the production near'
    assert 'ALWorldShadow::tileSlot(job.slot, face)' in cube
    copy = 'glCopyImageSubData(mWorldShadowScratch.getDepth(),GL_TEXTURE_2D,0,0,0,0,mWorldShadowAtlas.getDepth(),GL_TEXTURE_2D,0,tile.x,tile.y,0,ALWorldShadow::kTile,ALWorldShadow::kTile,1);'
    assert copy in re.sub(r'\s+','',cube), 'viewer atlas copy differs from the real-GL reproduction'
    assert 'AttachmentVisibilityScope attachment_visibility(' in cube
    assert cube.index('AttachmentVisibilityScope') < cube.index('renderShadow(view, proj, shadow_cam')
    for condition in ('!gAgentCamera.getCameraAnimating()', 'CAMERA_MODE_MOUSELOOK', 'LLVOAvatar::sVisibleInFirstPerson',
                      'const ECameraMode attachment_mode = gAgentCamera.getCameraMode();', 'attachment_mode, CAMERA_MODE_THIRD_PERSON,',
                      'updateAttachmentVisibility(mode)'):
        assert condition in cube, 'native attachment scope lost '+condition
    assert 'gAgentCamera.getCameraMode()' not in cube[cube.index('updateAttachmentVisibility(mode)'):cube.index('const glm::mat4 saved_proj')], 'the restore mode must be captured at construction, not read at exit'
    assert 'gSnapshot ||' in cube and 'ALWorldShadow::plannedLights(' in residency
    # Round 7's pin release stays (an UNPICKED projector that samples a native map releases its pin) and a PICKED one is exempt:
    # the release is one rule, ALWorldShadow::worldEligibleWithNative, exercised by unit test 41.
    assert 'spotShadowMapSampled(slot)) { native_sampled = true; break; }' in residency, 'native sampling must still be detected'
    assert 'return ALWorldShadow::worldEligibleWithNative(native_sampled,isWorldShadowPicked(id));' in residency, 'pin release: unpicked releases, picked is exempt'
    assert 'inline bool worldEligibleWithNative(bool nativeSampled, bool picked) { return picked || !nativeSampled; }' in (REPO / 'indra/newview/alworldshadowmath.h').read_text()
    assert 'native_mapped && !ALWorldShadow::worldEligibleWithNative(native_mapped, isWorldShadowPicked(mWorldShadowResident.id[k]))' in cube, 'raster loop: a picked projector keeps its cube'
    assert 'mWorldShadowResident.held[k] = false;' in cube and 'mWorldShadowFrame[k] = 0;' in cube
    for signature,end in [('void LLPipeline::releaseGLBuffers()', 'void LLPipeline::releaseLUTBuffers()'),
                          ('void LLPipeline::releaseShadowBuffers()', 'void LLPipeline::releaseScreenBuffers()')]:
        assert 'releaseWorldShadowTargets();' in body(signature,end), 'world targets missing from teardown'
    assert 'mWorldShadowResident =' not in release and 'stamp = 0' in release, 'teardown must invalidate stamps and preserve session pins'
    # Idempotent release; the allocation retry keeps the failure latch (one warning per failure streak), explicit teardown / OFF / success clear it.
    assert 'target->getFBO() || target->getDepth() || target->isComplete()' in release and 'if (clear_failure) mWorldShadowAllocFailed = false;' in release, 'release must be idempotent and honour the latch'
    assert cube.count('releaseWorldShadowTargets(false);') == 2 and 'mWorldShadowAllocFailed = false;' in cube, 'retry path must keep the failure latch and success must clear it'
    # Snapshots sample the last live frame's stamps instead of dropping the world channel.
    owned = body('bool LLPipeline::worldShadowOwned(', 'void LLPipeline::renderAreaShadows(')
    assert 'gSnapshot ? mWorldShadowLiveFrame : gFrameCount' in owned and 'mWorldShadowLiveFrame = gFrameCount;' in cube, 'snapshot stamp sampling lost'
    assert 'if (gSnapshot || !mWorldShadowAtlas' not in owned, 'a snapshot must not drop the world channel'
    assert 'llmin(source > 0.f ? source : .1f,2.f)' in pipe, 'production softbox radius clamp regressed'
    obj = (REPO / 'indra/newview/llviewerobject.cpp').read_text()
    draw = (REPO / 'indra/newview/lldrawable.cpp').read_text()
    objh = (REPO / 'indra/newview/llviewerobject.h').read_text()
    drawh = (REPO / 'indra/newview/lldrawable.h').read_text()
    octh = (REPO / 'indra/newview/llvieweroctree.h').read_text()
    octc = (REPO / 'indra/newview/llvieweroctree.cpp').read_text()
    # The revision: seeded process-unique at construction (a static linkset is cacheable at once), stamped O(1) through the shared header.
    assert 'U64 mWorldShadowBoundsRevision = nextWorldShadowRevision();' in objh, 'revision must be seeded at construction'
    fn = obj[obj.index('void LLViewerObject::invalidateWorldShadowBounds()'):]; fn = fn[:fn.index('\n}\n')]
    assert 'ALWorldShadow::stampBounds(' in fn and 'getRootEdit()' in fn and 'static U64 serial' not in fn, 'invalidation must be the O(1) shared stamp'
    assert 'ALWorldShadow::cachedNear(' in body('static F32 worldShadowLightNear(', '// Round 5: a human label') and 'revision > 1' not in pipe, 'the near cache must key on any valid revision'
    assert 'ALWorldShadow::linksetSiblingReach(' in body('static F32 worldShadowLightNearUncached(', 'static F32 worldShadowLightNear('), 'the uncapped shared sibling walk lost'
    # EVERY write of an octree entry's extents goes through the one hook, whatever the caller's static type: the base setters and shift() all
    # call it, LLDrawable overrides it, and no other translation unit writes entry extents.
    assert 'virtual void onSpatialExtentsChanged()' in octh and 'void onSpatialExtentsChanged() override;' in drawh, 'extents hook declaration lost'
    for signature in ('void LLViewerOctreeEntryData::setSpatialExtents(const LLVector3&', 'void LLViewerOctreeEntryData::setSpatialExtents(const LLVector4a&',
                      'void LLViewerOctreeEntryData::shift('):
        start = octc.index(signature); text_fn = octc[start:octc.index('\n}\n',start)]
        assert 'onSpatialExtentsChanged();' in text_fn, 'extents write without the hook: '+signature
    hook = draw[draw.index('void LLDrawable::onSpatialExtentsChanged()'):]; hook = hook[:hook.index('\n}\n')]
    assert 'invalidateWorldShadowBounds();' in hook, 'LLDrawable hook must invalidate'
    for source in (REPO / 'indra/newview').glob('*.cpp'):
        if source.name == 'llvieweroctree.cpp': continue
        assert 'mEntry->mExtents' not in source.read_text(errors='ignore'), 'another writer of entry extents: '+source.name
    for signature in ('bool LLViewerObject::setParent(', 'void LLViewerObject::addChild(', 'void LLViewerObject::removeChild('):
        start = obj.index(signature); fn = obj[start:obj.index('\n}\n',start)]
        assert 'invalidateWorldShadowBounds();' in fn, 'linkset invalidation lost '+signature
    for signature in ('F32 LLDrawable::updateXform(', 'void LLDrawable::shiftPos('):
        start = draw.index(signature); fn = draw[start:draw.index('\n}\n',start)]
        assert 'invalidateWorldShadowBounds();' in fn, 'drawable invalidation lost '+signature
    return [dict(name=n,passed=True) for n in ('attachment_scope_integration','native_map_pin_release','picked_projector_pin_release_exemption',
            'complete_linkset_cache_invalidation','GL_teardown','snapshot_pin_preservation','production_softbox_radius',
            'production_packing_and_extents_wiring','production_atlas_copy_wiring')]


def build_host(out, glm_include):
    if not glm_include:
        # Reuse installed dependencies, read-only; never invoke a viewer build.
        candidates = list(REPO.glob('build-*/vcpkg_installed/*/include'))
        cache = Path.home() / 'cxs/zs_flags_cache_cube.json'
        if cache.exists():
            candidates += [Path(p) for p in re.findall(r'/external:I \\"([^"\\]+)', cache.read_text())]
        glm_include = next((p for p in candidates if (p / 'glm/glm.hpp').exists()), None)
    assert glm_include and (Path(glm_include) / 'glm/glm.hpp').exists(), 'FAIL: provide --glm-include (installed GLM headers)'
    src = REPO / 'scripts/code_tools/relight_world_gl_host.cpp'
    exe = out / 'world_gl_host.exe'
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere), '-latest', '-property', 'installationPath'], text=True).strip()
    vcvars = Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
    cmd = out / 'build_host.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc /I "%s" /I "%s" "%s" /Fe:"%s" /Fo:"%s"\n' %
                   (vcvars, glm_include, REPO / 'indra/newview', src, exe, out / 'world_gl_host.obj'))
    r = subprocess.run([str(cmd)], cwd=out, capture_output=True, text=True, shell=True)
    assert r.returncode == 0, 'host compile FAIL:\n' + r.stdout + r.stderr
    return subprocess.Popen([str(exe)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)


def mesh(prim):
    """Outward triangle mesh; no emitter exclusion, including double-sided shells."""
    if prim.kind == 'obb':
        verts = []
        for axis in range(3):
            a, b = (axis + 1) % 3, (axis + 2) % 3
            for sign in (-1, 1):
                q = []
                for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    p = np.zeros(3); p[axis] = sign; p[a] = x; p[b] = y
                    q.append(p * prim.h)
                if sign < 0: q.reverse()
                verts += [q[i] for i in (0, 1, 2, 0, 2, 3)]
        return np.asarray(verts) @ prim.rot().T + prim.c
    # Capsule/sphere, 64x48: tessellated actual geometry, never analytic shadow depth.
    a = np.array(prim.a if prim.kind == 'capsule' else prim.c)
    b = np.array(prim.b if prim.kind == 'capsule' else prim.c)
    assert np.linalg.norm((b - a)[:2]) < 1e-8, 'fixture capsule is vertical'
    rings = []
    for theta in np.linspace(0, np.pi, 49):
        centre = b if theta <= np.pi / 2 else a
        rings.append([centre + prim.r * np.array([np.sin(theta)*np.cos(phi), np.sin(theta)*np.sin(phi), np.cos(theta)])
                      for phi in np.linspace(0, 2*np.pi, 65)])
    verts = []
    for j in range(48):
        for i in range(64):
            verts.extend([rings[j][i], rings[j+1][i], rings[j+1][i+1], rings[j][i], rings[j+1][i+1], rings[j][i+1]])
    return np.asarray(verts)


class Raster:
    def __init__(self, runner, caster='static'):
        self.r, self.gl = runner, runner.gl
        self.caster = caster
        gl = self.gl
        self.program = gl.program()
        vertex, fragment = {
            'static': ('shadowV', 'shadowF'),
            'avatar': ('avatarShadowV', 'avatarShadowF'),
            'rigged': ('shadowSkinnedV', 'shadowF'),
            'rigged_blend': ('shadowAlphaMaskV', 'shadowAlphaMaskF'),
        }[caster]
        modules = [('class1/deferred/'+vertex+'.glsl', 0x8B31), ('class1/deferred/'+fragment+'.glsl', 0x8B30),
                   ('class1/alchemy/actorFxDissolveF.glsl', 0x8B30)]
        if caster != 'static':
            modules.append(('class1/avatar/'+('avatarSkinV' if caster == 'avatar' else 'objectSkinV')+'.glsl', 0x8B31))
        if caster == 'rigged_blend':
            modules.append(('class1/objects/nonindexedTextureV.glsl', 0x8B31))
            # LLShaderMgr's one-channel diffuseLookup glue; sampled texture is
            # real RGBA, so fully transparent casters must be discarded.
            glue = 'uniform sampler2D tex0; vec4 diffuseLookup(vec2 uv) { return texture(tex0,uv); }'
            gl.attach(self.program, runner._compile(glue, 'diffuseLookup', 0x8B30))
        for path, stage in modules:
            source = runner.tree.read(G.SHADERS + '/' + path)
            if caster == 'rigged_blend': source = '#define HAS_SKIN 1\n' + source
            gl.attach(self.program, runner._compile(source, path, stage))
        gl.link(self.program)
        ok = gl.I(); gl.getprog(self.program, 0x8B82, C.byref(ok))
        assert ok.value, 'production shadow program link failed'
        self.enable = gl.fn('glEnable', None, gl.U)
        self.clear = gl.fn('glClear', None, gl.U)
        self.clear_depth = gl.fn('glClearDepth', None, C.c_double)
        self.depth_mask = gl.fn('glDepthMask', None, C.c_ubyte)
        self.depth_func = gl.fn('glDepthFunc', None, gl.U)
        self.draw_buffer = gl.fn('glDrawBuffer', None, gl.U)
        self.read_buffer = gl.fn('glReadBuffer', None, gl.U)
        self.copy = gl.fn('glCopyImageSubData', None, gl.U, gl.U, gl.I, gl.I, gl.I, gl.I,
                          gl.U, gl.U, gl.I, gl.I, gl.I, gl.I, gl.I, gl.I, gl.I)
        self.error = gl.fn('glGetError', gl.U)
        self.fbo = gl.U(); gl.gen_fbo(1, C.byref(self.fbo))
        self.vbo = gl.U(); gl.gen_buf(1, C.byref(self.vbo))
        self.constant_attr = gl.fn('glVertexAttrib4f', None, gl.U, gl.F, gl.F, gl.F, gl.F)
        self.disable_attr = gl.fn('glDisableVertexAttribArray', None, gl.U)
        self.palette = gl.fn('glUniformMatrix3x4fv', None, gl.I, gl.I, C.c_ubyte, C.c_void_p)

    def atlas(self, scene, light, near, alpha=1.):
        gl, r = self.gl, self.r
        scratch = r._texture('raster_scratch', 5, 0x8CAC, CS.TILE, CS.TILE, 0x1902, 0x1406, None)
        atlas = r._texture('raster_atlas', 6, 0x8CAC, CS.ATLAS_W, CS.ATLAS_H_PER_LIGHT, 0x1902, 0x1406, None)
        gl.bind_fbo(0x8D40, self.fbo)
        gl.fbo_tex(0x8D40, 0x8D00, 0x0DE1, scratch, 0)
        self.draw_buffer(0); self.read_buffer(0)
        assert gl.fbo_status(0x8D40) == 0x8CD5, 'D32F framebuffer incomplete'
        self.enable(0x0B71); self.depth_mask(1); self.depth_func(0x0201)
        for cap in (0x0C11, 0x864F, 0x8F9D): gl.disable(cap)  # scissor, polygon offset, depth clamp
        gl.viewport(0, 0, CS.TILE, CS.TILE)
        gl.use(self.program)
        gl.ui(gl.loc(self.program, b'actorFxEnabled'), 0)
        if self.caster != 'static':
            for name, values in [(b'weight', (0,0,0,0)), (b'weight4', (.5,0,0,0)),
                                 (b'diffuse_color', (1,1,1,1)), (b'texcoord0', (0,0,0,1))]:
                idx = gl.attr_loc(self.program, name)
                if idx >= 0:
                    self.disable_attr(idx); self.constant_attr(idx, *values)
        if self.caster == 'rigged_blend':
            r._texture('caster_alpha', 0, 0x8058, 1, 1, 0x1908, 0x1406, np.array([1,1,1,alpha], np.float32))
            gl.ui(gl.loc(self.program, b'tex0'), 0)
            gl.uf[1](gl.loc(self.program, b'minimum_alpha'), .598)  # renderAlphaObjects native blend cutoff
            gl.uf[1](gl.loc(self.program, b'shadow_target_width'), CS.TILE)
            r._mat(self.program, b'texture_matrix0', np.eye(4))
        far = light.range
        P = np.zeros((4, 4)); P[0, 0] = P[1, 1] = 1 / CS.TAN_HALF
        P[2, 2] = -(far+near)/(far-near); P[2, 3] = -2*far*near/(far-near); P[3, 2] = -1
        for face in range(6):
            axis, right, up = CS.face_basis(face)
            V = np.eye(4); V[:3, :3] = [right, up, -axis]; V[:3, 3] = -V[:3, :3] @ light.pos
            r._mat(self.program, b'modelview_projection_matrix', P @ V)
            if self.caster != 'static':
                r._mat(self.program, b'projection_matrix', P)
                r._mat(self.program, b'modelview_matrix', V)
                if self.caster == 'avatar':
                    # Classic avatar palettes include the current face view.
                    palette = np.zeros((45,4), np.float32); palette[[0,15,30]] = V[:3]
                    gl.u4fv(gl.loc(self.program, b'matrixPalette'), 45, palette.ctypes.data_as(C.POINTER(gl.F)))
                else:
                    palette = np.ascontiguousarray(np.eye(4)[:3], np.float32)
                    self.palette(gl.loc(self.program, b'matrixPalette'), 1, 0, palette.ctypes.data)
            self.clear_depth(1.0); self.clear(0x100)
            for prim in scene.prims:
                self.enable(0x0B44) if prim.cull == 'back' else gl.disable(0x0B44)
                data = np.ascontiguousarray(mesh(prim), np.float32)
                gl.bind_buf(0x8892, self.vbo); gl.buffer_data(0x8892, data.nbytes, data.ctypes.data, 0x88E4)
                idx = gl.attr_loc(self.program, b'position'); gl.enable_attr(idx); gl.attr_ptr(idx, 3, 0x1406, 0, 0, None)
                gl.draw(4, 0, len(data))
            self.copy(scratch, 0x0DE1, 0, 0, 0, 0, atlas, 0x0DE1, 0, (face % 3)*CS.TILE, (face // 3)*CS.TILE, 0, CS.TILE, CS.TILE, 1)
            assert self.error() == 0, 'GL error during face raster/copy'
        # Resolve binds the actual GPU atlas, not a CPU readback/re-upload.
        gl.disable(0x0B71); gl.disable(0x0B44)
        gl.bind_fbo(0x8D40, r.fbo)
        return atlas


class RealRunner(G.AreaRunner):
    def __init__(self, tree, host, gl=None):
        super().__init__(tree,gl=gl)
        self.host, self.old_packing = host, False
        self.gpu_atlas = None

    def _texture(self, key, unit, internal, w, h, fmt, typ, data, nearest=True):
        if key == 'atlas' and self.gpu_atlas is not None:
            self.gl.active(0x84C0 + unit); self.gl.bind_tex(0x0DE1, self.gpu_atlas)
            return self.gpu_atlas
        return super()._texture(key, unit, internal, w, h, fmt, typ, data, nearest)

    def packed(self, view):
        self.host.stdin.write(' '.join(map(str, view.ravel())) + '\n'); self.host.stdin.flush()
        vals = np.array(list(map(float, self.host.stdout.readline().split())), np.float32)
        assert vals.size == 18, 'C++ packing helper failed'
        correct, old = vals[:9], vals[9:]
        assert np.allclose(correct, G.pack_view_rotation(view), atol=1e-7), 'host and harness packing differ'
        return old if self.old_packing else correct

    def run(self, cam, *args, **kwargs):
        packed = self.packed(cam.view)
        pack = G.pack_view_rotation
        G.pack_view_rotation = lambda _: packed
        try: return super().run(cam, *args, **kwargs)
        finally: G.pack_view_rotation = pack


class Probe:
    """Isolate the unchanged production awVisibility at exact world receivers.

    This removes pixel-centre interpolation from the micrometre seam measurement;
    raster, atlas, packing and every resolve function remain production code.
    """
    def __init__(self, runner):
        self.r = runner
        text = runner.tree.read(G.SHADERS + '/class3/deferred/areaShadowFilterF.glsl')
        body = '''uniform sampler2D probe_positions;
uniform int probe_plane;
void main() {
    area_p = texelFetch(probe_positions,ivec2(gl_FragCoord.xy),0).xyz;
    area_n_geo = probe_plane != 0 ? vec3(0,0,1) : -normalize(area_p-area_world_light[0].xyz);
    frag_color = vec4(vec3(awVisibility(0)),1);
}'''
        self.program = runner._link('class3/deferred/areaShadowFilterF.glsl', [(text, text[:text.index('void main()')] + body)])

    def run(self, positions, light, near, plane=False, view=None):
        """view: optional 4x4 rotation (view = agent -> view space, no translation). Positions and the light go through it and the
        production packed rotation carries it back, so the resolve sees a rotated camera exactly as the viewer feeds it."""
        r, pr = self.r, self.program
        gl = r.gl
        view = np.eye(4) if view is None else np.asarray(view, np.float64)
        positions = np.asarray(positions, np.float64).reshape(-1, 3) @ view[:3, :3].T
        light_pos = view[:3, :3] @ np.asarray(light.pos, np.float64)
        data = np.asarray(positions, np.float32).reshape(1, -1, 3)
        r._texture('probe_positions', 7, 0x8815, data.shape[1], 1, 0x1907, 0x1406, data)
        r._target('probe_output', data.shape[1], 1); r._begin(pr)
        gl.ui(gl.loc(pr, b'probe_positions'), 7); gl.ui(gl.loc(pr, b'probe_plane'), int(plane))
        gl.ui(gl.loc(pr, b'area_world_atlas'), 4)
        gl.active(0x84C4); gl.bind_tex(0x0DE1, r.gpu_atlas)
        info = np.zeros((4, 4), np.float32); info[0] = (0, near, light.radius, 0)
        lights = np.zeros((4, 4), np.float32); lights[0] = (*light_pos, light.range)
        seed = np.array([light.seed, 0, 0, 0], np.uint32)
        rot = r.packed(view)
        for name, value in [(b'area_world_info', info), (b'area_world_light', lights)]:
            gl.u4fv(gl.loc(pr, name), 4, value.ctypes.data_as(C.POINTER(gl.F)))
        gl.u4uiv(gl.loc(pr, b'area_world_seed'), 1, seed.ctypes.data_as(C.POINTER(gl.U)))
        gl.u3fv(gl.loc(pr, b'area_world_rot'), 3, rot.ctypes.data_as(C.POINTER(gl.F)))
        gl.draw(4, 0, 3); gl.finish()
        result = np.empty((1, data.shape[1], 4), np.uint8)
        gl.read(0, 0, data.shape[1], 1, 0x1908, 0x1401, result.ctypes.data)
        return result[0, :, 0].astype(float)/255


SEAM_LIMIT = 2.00001    # 2/255
SEAM_RADII = (.1, 1., 2.)


def rotated_view():
    """A fixed, nontrivial camera rotation (yaw/pitch/roll), no translation: the production packing carries it back to the agent frame."""
    y, p, q = np.radians([37.3, -21.7, 11.1])
    rz = np.array([[np.cos(y), -np.sin(y), 0], [np.sin(y), np.cos(y), 0], [0, 0, 1]])
    ry = np.array([[np.cos(p), 0, np.sin(p)], [0, 1, 0], [-np.sin(p), 0, np.cos(p)]])
    rx = np.array([[1, 0, 0], [0, np.cos(q), -np.sin(q)], [0, np.sin(q), np.cos(q)]])
    view = np.eye(4); view[:3, :3] = rz @ ry @ rx
    return view


SEAM_VIEWS = (('identity', np.eye(4)), ('rotated', rotated_view()))


class MutantTree(G.SourceTree):
    """The worktree with named substitutions applied to areaShadowFilterF.glsl: a CONTROL, never production text. A control is only
    meaningful if its target text still exists, so a vanished target is a gate failure."""
    def __init__(self, base, subs):
        super().__init__(base.ref)
        self.subs = subs

    def read(self, path):
        text = super().read(path)
        if Path(path).name == 'areaShadowFilterF.glsl':
            for old, new in self.subs:
                assert old in text, 'mutation target vanished from the shader: ' + old
                text = text.replace(old, new)
        return text


def mutant(r, subs):
    """(runner, raster, probe) on the mutated shader text, sharing the GL context and host."""
    m = RealRunner(MutantTree(r.tree, subs), r.host, gl=r.gl)
    return m, Raster(m), Probe(m)


# The pre-fix behaviours, as shader-text controls. Each MUST be rejected by the check named in main().
MUT_HARD_FACE = [('vec3 w = smoothstep(vec3(AW_OVERLAP*major),vec3(major),a);',
                  'vec3 w = vec3(a.x >= a.y && a.x >= a.z ? 1.0 : 0.0, (a.x < a.y && a.y >= a.z) ? 1.0 : 0.0, (a.x < a.z && a.y < a.z) ? 1.0 : 0.0);')]
MUT_NO_PLANE_GUARD = [('vec4 plane = mix(vec4(zr),clamp(vec4(nd)/min(dens,vec4(-1.e-3)),vec4(zr-cap),vec4(zr+cap)),vec4(lessThan(dens,vec4(-1.e-3))));',
                       'vec4 plane = vec4(nd)/min(dens,vec4(-1.e-5));')]
MUT_WIDE_OVERLAP = [('const float AW_OVERLAP = 0.95;', 'const float AW_OVERLAP = 0.5;')]
# COST upper bound only (relight_world_cost.py): every tap reads all three faces, as if every receiver ray sat on a cube corner.
MUT_ALL_FACES = [('const float AW_OVERLAP = 0.95;', 'const float AW_OVERLAP = 0.0;')]
MUT_TWO_FACES = [('vec3 w = smoothstep(vec3(AW_OVERLAP*major),vec3(major),a);', 'vec3 w = vec3(1.0,1.0,0.0);')]   # every tap reads two faces, as if every receiver ray sat on a cube edge
MUT_WIDE_UNCLAMPED = MUT_WIDE_OVERLAP + [('vec2 q = clamp(centre-0.5,vec2(0.0),vec2(AW_TILE-2.0));', 'vec2 q = centre-0.5;')]


def seam_checks(r, raster, probe):
    """Face-boundary continuity of the production resolve at micrometre spacing, for 0.1, 1 and 2 m sources and an identity and a rotated
    camera. The atlas does not depend on the source radius, so each scene is rasterised once. Rows: the historical per-edge / per-corner
    rows (0.1 m, identity) plus one aggregate row per (feature class, radius, view)."""
    detail, worst = {}, {}
    def record(kind, key, radius, vname, delta):
        worst[(kind, radius, vname)] = max(worst.get((kind, radius, vname), 0.), delta)
        if radius == .1 and vname == 'identity':
            detail[(kind, key)] = max(detail.get((kind, key), 0.), delta)
    def lights(radius): return S.Light('seam', (0, 0, 0), radius, 10., seed=123)
    # All 12 signed edges, sampled along the whole edge including both corner
    # approaches; a sphere silhouette deliberately places the probes in penumbra.
    for omitted in range(3):
        axes = [i for i in range(3) if i != omitted]
        for s0 in (-1, 1):
            for s1 in (-1, 1):
                for along in (-.99, -.6, 0., .6, .99):
                    d = np.zeros(3); d[axes] = (s0, s1); d[omitted] = along
                    d = S.unit(d)
                    tangent = np.zeros(3); tangent[omitted] = 1; tangent = S.unit(tangent-d*np.dot(tangent,d))
                    sphere = S.Prim('sphere', 'sphere', c=tuple(2*d+.37*tangent), r=.4)
                    sc = S.Scene('seam', [sphere], [[lights(.1)]], [], {}, np.empty((0,3)))
                    r.gpu_atlas = raster.atlas(sc, lights(.1), .05)
                    points = []
                    for offset in np.linspace(-.24,.24,33):
                        p = 4*d+offset*tangent
                        step = np.zeros(3); step[axes] = (s0*1.e-6,-s1*1.e-6)
                        points.extend([p-step,p+step])
                    for radius in SEAM_RADII:
                        for vname, view in SEAM_VIEWS:
                            v = probe.run(points, lights(radius), .05, view=view).reshape(-1,2)
                            record('edge', (omitted, s0, s1), radius, vname, float(np.max(np.abs(v[:,0]-v[:,1])))*255)
    for sx in (-1,1):
        for sy in (-1,1):
            for sz in (-1,1):
                d = S.unit([sx,sy,sz]); t = S.unit(np.cross(d,[0,0,1]))
                for offset in np.linspace(.28,.48,21):
                    sc = S.Scene('corner', [S.Prim('sphere','sphere',c=tuple(2*d+offset*t),r=.4)], [[lights(.1)]], [], {}, np.empty((0,3)))
                    r.gpu_atlas = raster.atlas(sc,lights(.1),.05)
                    points = [4*d + np.eye(3)[i]*[sx,sy,sz][i]*1.e-6 for i in range(3)]
                    for radius in SEAM_RADII:
                        for vname, view in SEAM_VIEWS:
                            record('corner', (sx,sy,sz), radius, vname, float(np.ptp(probe.run(points,lights(radius),.05,view=view)))*255)
    rows = []
    for omitted in range(3):
        for s0 in (-1, 1):
            for s1 in (-1, 1):
                delta = detail[('edge', (omitted, s0, s1))]
                rows.append(dict(name='edge_%d_%d_%d' % (omitted,s0,s1), delta_255=delta, passed=delta <= SEAM_LIMIT))
    for sx in (-1,1):
        for sy in (-1,1):
            for sz in (-1,1):
                delta = detail[('corner', (sx,sy,sz))]
                rows.append(dict(name='corner_%d_%d_%d' % (sx,sy,sz), delta_255=delta, passed=delta <= SEAM_LIMIT))
    for (kind, radius, vname), delta in sorted(worst.items()):
        if radius == .1 and vname == 'identity': continue      # the per-feature rows above
        rows.append(dict(name='%ss_r%g_%s' % (kind, radius, vname), delta_255=delta, passed=delta <= SEAM_LIMIT))
    return rows


def codex_corner(r, raster, probe, radius=2., view=None):
    """Review round 5's exact reproduction: seed 123, light at the origin, near 0.05, a radius 0.4 sphere at (-1.437543, -0.871858, -1.154701),
    receivers 1 micrometre from 4 * normalize(-1,-1,-1) along each of the three dominant axes. Returns the 3 visibilities in 1/255."""
    d = S.unit([-1,-1,-1])
    centre = 2*d+.4*S.unit(np.cross(d,[0,0,1]))
    assert np.allclose(centre, (-1.437543,-0.871858,-1.154701), atol=2.e-6), 'fixture no longer the reviewed one'
    light = S.Light('corner', (0,0,0), radius, 10., seed=123)
    r.gpu_atlas = raster.atlas(S.Scene('corner', [S.Prim('sphere','sphere',c=tuple(centre),r=.4)], [[light]], [], {}, np.empty((0,3))), light, .05)
    return probe.run([4*d-np.eye(3)[i]*1.e-6 for i in range(3)], light, .05, view=view)*255


def skinned_caster_checks(r, probe):
    """Production skinning + shadow programs -> D32F cube -> production resolve.

    Identity joints, a thin upright box 0.3..2 m above a floor, and a light 4 m
    away. Rotate the fixture through all six faces. No system body or static
    caster can accidentally supply a missing mesh-body shadow.
    """
    rows = []
    for kind in ('avatar', 'rigged', 'rigged_blend'):
        raster = Raster(r, caster=kind)
        blocked, lit = [], []
        for face in range(6):
            axis, right, up = CS.face_basis(face)
            # Local +Z is floor normal; map local +X into this cube face.
            rotation = np.column_stack([axis, -right, up])
            assert np.isclose(np.linalg.det(rotation), 1.), 'fixture must rotate, not reflect'
            light = S.Light('skin', tuple(rotation @ np.array([0.,0.,2.5])), .1, 12., seed=123)
            box = S.Prim('caster', 'obb', c=tuple(rotation @ np.array([4.,0.,1.15])), h=(.18,.3,.85))
            # mesh() accepts arbitrary OBB rotation through rot(). This fixture
            # supplies its rigid orientation without changing shader source.
            box.rot = lambda rotation=rotation: rotation
            sc = S.Scene('skin', [box], [[light]], [], {}, np.empty((0,3)))
            points = np.array([[6.,0.,0.], [6.,2.,0.]]) @ rotation.T
            # Put this face's floor normal at view +Z, as Probe's plane mode
            # expects. Production camera packing rotates it back to the atlas.
            view = np.eye(4); view[:3,:3] = rotation.T
            r.gpu_atlas = raster.atlas(sc, light, .05)
            v = probe.run(points, light, .05, plane=True, view=view)
            blocked.append(float(v[0])); lit.append(float(v[1]))
        rows.append(dict(name='production_'+kind+'_six_faces', blocked_max=max(blocked), lit_min=min(lit),
                         passed=bool(max(blocked) < .1 and min(lit) > .99)))
        if kind == 'rigged_blend':
            r.gpu_atlas = raster.atlas(sc, light, .05, alpha=0.)
            v = probe.run(points, light, .05, plane=True, view=view)
            rows.append(dict(name='production_rigged_blend_transparent_discard', min_visibility=float(min(v)), passed=bool(min(v) > .99)))
    return rows


def bleed_atlas(r, legit=(0, 4)):
    """A synthetic atlas: every texel is clear (window depth 1) except the tiles of the faces NOT in `legit`, which hold a blocker 0.1 m
    from the light. The legit faces' tiles (+X and +Z) are the only data a receiver on the +X/+Z cube edge may read, so any darkening is a
    read of another face's tile through the atlas neighbours: right of the +Z tile (face 4, row 1) is the -Z tile (face 5)."""
    data = np.ones((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32)
    for face in range(6):
        if face not in legit:
            x, y = (face % 3)*CS.TILE, (face // 3)*CS.TILE
            data[y:y+CS.TILE, x:x+CS.TILE] = .5
    return r._texture('raster_atlas', 6, 0x8CAC, CS.ATLAS_W, CS.ATLAS_H_PER_LIGHT, 0x1902, 0x1406, data)


def bleed_value(r, probe, radius=.3):
    """Minimum visibility of receivers along the +X/+Z cube edge (and +-Y offsets), source `radius` (the direction path, footprint <= 0.14 rad,
    so only the +X and +Z faces are ever legitimately read). 1 = no darkening."""
    light = S.Light('bleed', (0,0,0), radius, 10., seed=123)
    r.gpu_atlas = bleed_atlas(r)
    pts = []
    for az in np.linspace(-.02, .02, 9):          # radians either side of the 45 degree edge plane
        for el in np.linspace(-.08, .08, 9):
            a = np.pi/4+az
            pts.append(4*np.array([np.cos(a)*np.cos(el), np.sin(el), np.sin(a)*np.cos(el)]))
    return float(np.min(probe.run(pts, light, .05)))


def softbox_check(r, raster, probe, radius):
    """A half-plane blocker under a source of the given radius: the 10-90 % width against the analytic uniform-disk straight edge, the rms
    against its full CDF, and the umbra (no leak through the halved blocker search the large-source path uses)."""
    light = S.Light('softbox_%gm' % radius,(0,0,3.5),radius,10.,seed=123)
    blocker = S.Prim('half_plane','obb',c=(-10,0,1.49),h=(10,10,.01),cull='none')
    sc = S.Scene('softbox',[blocker],[[light]],[],{},np.empty((0,3)))
    r.gpu_atlas = raster.atlas(sc,light,.05)
    x = np.linspace(-4,4,1601)
    v = probe.run(np.column_stack([x,x*0,x*0]),light,.05,True)
    # The receiver at x sees source-disk points sx > k*|x|... (k = 3.5/1.49 - 1) for x < 0: the visible disk fraction is the analytic CDF.
    u = np.clip(-(3.5/1.49-1)*x/radius, -1, 1)
    analytic = (np.arccos(u)-u*np.sqrt(1-u*u))/np.pi
    # A uniform disk's analytic straight-edge CDF; 10..90% spans 1.374 of its projected radius R*(receiver-blocker)/blocker.
    expected = 2*.687048826*radius*.75
    smooth = np.convolve(np.pad(v,8,mode='edge'),np.ones(17)/17,mode='valid')
    lo = np.interp(.1,np.maximum.accumulate(smooth),x)
    hi = np.interp(.9,np.maximum.accumulate(smooth),x)
    width = float(hi-lo)
    rms = float(np.sqrt(np.mean((v-analytic)**2)))
    umbra = float(np.max(v[analytic < 1.e-3]))
    lit = float(np.min(v[analytic > 1-1.e-3]))
    ok = abs(width/expected-1) <= .2 and rms <= .05 and umbra <= .02 and lit >= .98
    return dict(name='softbox_%gm' % radius if radius != 1 else 'softbox_1m',width_m=width,physical_width_m=expected,error_fraction=abs(width/expected-1),
                rms_vs_analytic=rms,umbra_max=umbra,lit_min=lit,passed=bool(ok))


GRAZING_LIMIT = .25      # percent of the visible floor reading dark (false dark) on an EMPTY floor


def grazing_value(r, raster, radius):
    """Worst false-dark percentage on an empty floor lit by a source of `radius` 5 cm and 20 cm above it, so every receiver within 8 m is
    seen at a grazing angle (1.4 degrees at 2 m... 0.3 degrees at 8 m), over six camera views. A 2 m source fans its footprint 43 degrees."""
    worst = 0.
    for height in (.05, .2):
        prims = [S.Prim('floor', 'obb', c=(0, 0, -.1), h=(15, 15, .1))]
        light = S.Light('lamp', (0., 0., height), radius, 10., seed=123)
        sc = S.Scene('graze', prims, [[light]], [], {}, np.zeros((0, 3)))
        for yaw, pitch in [(-54.462, 28.635), (0, 20), (45, 35), (90, 60), (135, 25), (225, 45)]:
            worst = max(worst, evaluate(r, raster, sc, light, yaw, pitch)[2])
    return worst


def planner_control(out):
    """The gate must reject the captured fallback table: force the planner adapter's compile to be unavailable (no MSVC, no cache) and require
    that the production-planner requirement raises."""
    saved = PL._vcvars
    PL._vcvars = lambda: None
    try:
        result = PL.compute_plans_status(G.SourceTree('WORKTREE'), Path(out) / 'plan_control')
    finally:
        PL._vcvars = saved
    assert not result.compiled and result.source.startswith('FALLBACK'), 'control did not reach the fallback table'
    try:
        PL.require_compiled(result)
    except AssertionError:
        return True
    return False


def flicker_checks(r, raster, out):
    """The original B_camorbit_low orbit at 1080p, using real raster depth.

    Same world probes, disk ground truth, 95th percentile and visibility sampling
    as the 3D acceptance harness. Require BOTH production quality plans.
    """
    sc = S.scene_sphere()
    poses = [p for p in sc.poses if p.group == 'camorbit_low']
    assert len(poses) == 12
    light = sc.light_sets[poses[0].cfg][0]
    near = CS.emitter_near(sc, light)
    r.gpu_atlas = raster.atlas(sc, light, near)
    # The captured fallback table is NOT the production planner: a failed adapter compile fails the gate (also for a cached table, which only
    # a successful compile ever writes). planner_control() proves this requirement rejects the fallback.
    plans = PL.require_compiled(PL.compute_plans_status(r.tree, out)).plans
    pts = sc.probes
    gt = S.trace_light(sc,light,pts,np.tile([0.,0.,1.],(len(pts),1)),np.arange(len(pts),dtype=np.uint32),S=256)
    gt = (1.-gt['nblk'].astype(np.float32)/256)[None,:]
    seq = {cfg: [] for cfg in ('cube','cube+thin','cube_worldonly')}
    identical = True
    for pose in poses:
        cam = S.Camera(pose.eye,pose.target,1920,1080,pose.vfov)
        g = S.cast_primary(sc,cam)
        _,depth,norm = S.encode_gbuffer(cam,g)
        xy = T.probe_projection(sc,cam,g)
        for cfg in seq:
            world = {'atlas': np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),'channels':
                     [dict(row=0,near=near,far=light.range,r_map=light.radius,contact=0. if cfg == 'cube_worldonly' else CS.contact_distance(light.range),seed=light.seed)]}
            plan = plans['Default+Thin' if cfg == 'cube+thin' else 'Default'][1]
            v = r.run(cam,depth,norm,[light],plan,world=world)
            if cfg == 'cube': regular_image = v
            if cfg == 'cube+thin': identical &= np.array_equal(v,regular_image)
            _,pv = R.sample_full(v,(np.array([0]),np.array([0])),xy)
            seq[cfg].append(dict(probe=pv,cls=np.zeros_like(pv,dtype=np.uint8),cfg=0))
    rows = []
    for cfg, frames in seq.items():
        pairs = M.temporal(frames,[True]*len(frames),{0:gt})
        (out / ('flicker_pairs_'+cfg+'.json')).write_text(json.dumps(pairs,indent=2))
        assert len(pairs) == 11 and 'p95' in pairs[3], 'incomplete camera orbit coverage'
        measured = [p for p in pairs if 'p95' in p]
        assert len(measured) >= 6, 'too few shared visible probes in the orbit'
        worst = max(p['p95'] for p in measured); pop = max(p['pop'] for p in measured)
        row = dict(name='B_camorbit_low_'+cfg,p95=worst,pop=pop,pair_003_004=pairs[3]['p95'])
        # Acceptance is the two shipped configurations, as in cube_gates. The
        # contact-disabled debug path diagnoses raster/probe aliasing separately.
        if cfg != 'cube_worldonly': row['passed'] = worst<=.06 and pop<=.05
        rows.append(row)
    rows.append(dict(name='cube_vs_thin_full_images',frames=len(poses),passed=bool(identical)))
    # Resolve the contact-disabled diagnostic's worst 1080p pair again at 4K.
    # A world transform error survives this; pixel-centre interpolation aliasing
    # shrinks as the same physical penumbra spans more receiver pixels.
    worst_pair = max(measured,key=lambda p:p['p95'])
    high = []
    for pose in poses[worst_pair['i']:worst_pair['i']+2]:
        cam = S.Camera(pose.eye,pose.target,3840,2160,pose.vfov)
        g = S.cast_primary(sc,cam); _,depth,norm = S.encode_gbuffer(cam,g)
        xy = T.probe_projection(sc,cam,g)
        world = {'atlas':np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),'channels':
                 [dict(row=0,near=near,far=light.range,r_map=light.radius,contact=0.,seed=light.seed)]}
        v = r.run(cam,depth,norm,[light],plans['Default'][1],world=world)
        _,pv = R.sample_full(v,(np.array([0]),np.array([0])),xy)
        high.append(dict(probe=pv,cls=np.zeros_like(pv,dtype=np.uint8),cfg=0))
    hi = M.temporal(high,[True,True],{0:gt})[0]
    rows.append(dict(name='worldonly_resolution_control',pair=worst_pair['i'],p95_1080=worst_pair['p95'],p95_4k=hi['p95'],
                     passed=hi['p95'] < .8*worst_pair['p95']))
    return rows


def fixture(capsule=True, emitter=None, translated=False, projector=False):
    pos = (0., 0., 1.)
    prims = [S.Prim('floor', 'obb', c=(0, 0, -.1), h=(15, 15, .1))]
    if capsule: prims.append(S.Prim('capsule', 'capsule', a=(.85 if translated else 1.5, 0, .25), b=(.85 if translated else 1.5, 0, 1.55), r=.25))
    if emitter: prims.append(S.Prim('bulb', 'obb', c=pos, h=(.25, .25, .25), yaw=40, cull=emitter))
    light = S.Light('lamp', pos, .1, 10., ignore='bulb' if emitter else '', seed=123,
                    kind='projector' if projector else 'point', dir=(.612372, .612372, -.5))
    sc = S.Scene('realGL', prims, [[light]], [], {}, np.zeros((0, 3)))
    return sc, light


def evaluate(r, raster, scene, light, yaw=-54.462, pitch=28.635, contact=0., thin=False, channel=None):
    t, p = np.radians([yaw, pitch])
    cam = S.Camera((6*np.cos(p)*np.cos(t), 6*np.cos(p)*np.sin(t), 6*np.sin(p)), (0, 0, .4), 640, 360)
    g = S.cast_primary(scene, cam)
    _, depth, norm = S.encode_gbuffer(cam, g)
    near = CS.emitter_near(scene, light)
    if light.ignore:
        # An active emitter stores LOCAL extents. Send its render matrix and
        # extents through the production C++ transform/near helpers too.
        emitter = scene.prims[scene.prim_index(light.ignore)]
        matrix = np.eye(4); matrix[:3,:3] = emitter.rot().T; matrix[3,:3] = emitter.c
        values = [*matrix.ravel(), *(-np.array(emitter.h)), *emitter.h, *light.pos, light.range/1.5, .05]
        native = float(subprocess.check_output([r.host.args[0],'--bounds'],input=' '.join(map(str,values)),text=True))
        assert abs(native-near) < 1e-6, 'active emitter bounds/near differ between host and raster'
        near = native
    r.gpu_atlas = raster.atlas(scene, light, near)
    world = {'atlas': np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), 'channels':
             [dict(row=0, near=near, far=light.range, r_map=min(light.radius, 2.), contact=contact, seed=light.seed, **(channel or {}))]}
    plan = dict(samples=8, steps=12, half=True, near_steps=16 if thin else 0)
    v = r.run(cam, depth, norm, [light], plan, world=world)[0]
    mask = (g['surf'] == 0) & (np.linalg.norm(g['pos'][..., :2], axis=2) < 8.)
    assert np.count_nonzero(mask) > 1000, 'fixture has insufficient floor coverage'
    return v, mask, 100 * float(np.mean(v[mask] < .5))


def image_fixtures():
    """Illustrations only: no changes to the acceptance fixtures or their limits."""
    for name, title, kw in [
            ('01_point_capsule', 'Floor + 1.8 m capsule; point light 1 m above floor', {}),
            ('02_rotated_box', 'Capsule + 0.5 m box emitter, yaw 40 degrees (double-sided)', dict(emitter='none')),
            ('03_projector', 'Cube projector: pitch -30 degrees, yaw 45 degrees', dict(emitter='none', projector=True))]:
        sc, light = fixture(**kw)
        target = (1.4, .25, .35)
        if kw.get('projector'):
            # Stage the image's capsule inside the cone, so its floor shadow is
            # visible; the gate's projector fixture remains untouched.
            sc.prims[1].a = (1.1, 1.1, .25); sc.prims[1].b = (1.1, 1.1, 1.55)
            target = (1.6, 1.2, .35)
        yield name, title, sc, light, target, 5.8, CS.emitter_near(sc, light)
    sc, _ = fixture()
    light = S.Light('softbox', (0, 0, 3.5), 2., 10., seed=123)
    sc.light_sets = [[light]]
    yield '04_softbox_2m', 'Capsule under a 2 m source-radius softbox, height 3.5 m', sc, light, (1.6, .1, .4), 5.8, .05
    # Same light, sphere and receiver neighbourhood as codex_corner(). Add a
    # virtual receiver floor through its probes, for the perspective G-buffer
    # only. The original sphere-only shadow raster fixture stays exact.
    d = S.unit([-1, -1, -1]); centre = 2*d + .4*S.unit(np.cross(d, [0, 0, 1]))
    z = float(4*d[2])
    light = S.Light('corner', (0, 0, 0), 2., 10., seed=123)
    sc = S.Scene('corner_images', [
        S.Prim('floor', 'obb', c=(0, 0, z-.1), h=(15, 15, .1)),
        S.Prim('sphere', 'sphere', c=tuple(centre), r=.4)], [[light]], [], {}, np.empty((0, 3)))
    yield '05_three_face_corner', 'Three-face corner: reviewed sphere / light / receiver fixture; source radius 2 m', sc, light, (-2.1, -1.8, z+.3), 4.2, .05


def save_images(r, raster, directory):
    """Opt-in pictures from real raster/copies and the unmodified production resolve.

    The first column uses the gate's 256-ray disk oracle at each lit pixel.
    World-only sets the actual production debug uniform (not an approximation).
    Packing/debug overrides are reset; none of these values enter the gate rows.
    """
    from PIL import Image, ImageDraw, ImageFont

    directory = Path(directory).resolve(); directory.mkdir(parents=True, exist_ok=True)
    def font(size):
        try: return ImageFont.truetype('C:/Windows/Fonts/arial.ttf', size)
        except OSError: return ImageFont.load_default(size=size)
    heading, label, small = font(22), font(19), font(16)
    W, H, gap, top, footer = 640, 400, 8, 104, 98
    titles = ('Ray-traced ground truth', 'OLD broken packing', 'CURRENT production', 'World-only (debug = 1)', 'Error: CURRENT - truth')
    saved_packing = r.old_packing
    debug_loc = r.gl.loc(r.filter, b'area_world_debug')
    try:
        for name, title, sc, light, target, distance, near in image_fixtures():
            print('[world-gl images] rendering', name, flush=True)
            # Equal horizontal and vertical offsets give a 45 degree down view.
            az = np.radians(-65.)
            eye = np.array(target) + distance*np.array([np.cos(az), np.sin(az), 1.])
            cam = S.Camera(eye, target, W, H, vfov=52.)
            g = S.cast_primary(sc, cam); _, depth, norm = S.encode_gbuffer(cam, g)
            raster_scene = sc
            if name == '05_three_face_corner':
                raster_scene = S.Scene('corner', sc.prims[1:], [[light]], [], {}, np.empty((0, 3)))
            r.gpu_atlas = raster.atlas(raster_scene, light, near)
            world = {'atlas': np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), 'channels':
                     [dict(row=0, near=near, far=light.range, r_map=min(light.radius, 2.),
                           contact=CS.contact_distance(light.range), seed=light.seed)]}
            plan = dict(samples=8, steps=12, half=True, near_steps=0)
            def resolve(old=False, debug=0):
                r.old_packing = old
                r.gl.use(r.filter); r.gl.ui(debug_loc, debug)
                return r.run(cam, depth, norm, [light], plan, world=world)[0]
            old = resolve(old=True)
            current = resolve()
            world_only = resolve(debug=1)
            r.gl.use(r.filter); r.gl.ui(debug_loc, 0); r.old_packing = saved_packing

            hit = g['surf'] != 255
            pts, normals = g['pos'][hit], g['nrm'][hit]
            lighting, _ = S.unshadowed(light, pts, normals)
            active = lighting > 1.e-5
            truth = np.ones((H, W), np.float32)
            hit_truth = np.ones(len(pts), np.float32)
            hit_truth[active] = S.trace_light(sc, light, pts[active], normals[active],
                np.flatnonzero(hit).astype(np.uint32)[active], S=256)['vis']
            truth[hit] = hit_truth
            l0 = np.zeros((H, W), np.float32); l0[hit] = lighting
            albedo = np.zeros((H, W, 3), np.float32)
            for i, prim in enumerate(sc.prims):
                color = (.62, .66, .72) if prim.name == 'floor' else (.72, .39, .19)
                albedo[g['surf'] == i] = color
            def shaded(visibility):
                # A faint ambient fill keeps the geometry readable in full shadow.
                linear = albedo*(.035 + 1.35*l0*visibility)[..., None]
                rgb = np.clip(linear, 0, 1)**(1/2.2)
                rgb[~hit] = (.065, .08, .11)
                return np.uint8(np.clip(rgb[::-1]*255, 0, 255))
            error = np.zeros((H, W, 3), np.float32)
            delta = np.where(hit & (l0 > .001), current-truth, 0.)
            error[..., 0] = np.maximum(delta, 0)
            error[..., 2] = np.maximum(-delta, 0)
            panels = [shaded(v) for v in (truth, old, current, world_only)]
            panels.append(np.uint8(np.clip(error[::-1]*255, 0, 255)))
            sheet = Image.new('RGB', (5*W+6*gap, top+H+footer), '#141b27')
            draw = ImageDraw.Draw(sheet)
            draw.text((gap, 10), title, font=heading, fill='white')
            draw.text((gap, 40), 'REAL GL | %s | 6 x 544^2 depth faces -> GPU atlas copies -> production resolve | 45 deg perspective' % r.gl.renderer,
                      font=small, fill='#bdc9db')
            for i, (caption, pixels) in enumerate(zip(titles, panels)):
                x = gap+i*(W+gap)
                draw.text((x, 77), caption, font=label, fill='white')
                sheet.paste(Image.fromarray(pixels), (x, top))
            # Marker is a presentation overlay only, never caster geometry. In
            # the corner sheet its ring locates the exact three-face meeting.
            marker = 4*S.unit([-1, -1, -1]) if name == '05_three_face_corner' else np.array(light.pos)
            px, py, _ = cam.project(np.array([marker]))
            mx, my = float(px[0]), float(H-py[0])
            if 8 < mx < W-8 and 8 < my < H-8:
                for i in range(4):
                    x, y = gap+i*(W+gap)+mx, top+my
                    draw.ellipse((x-5, y-5, x+5, y+5), outline='#ffe19a', width=2)
            y = top+H+10
            draw.text((gap, y), 'Lighting columns: Lambert x distance falloff x visibility; same exposure and ambient fill. Truth: existing 256-ray disk oracle.', font=small, fill='#bdc9db')
            draw.text((gap, y+25), 'Error: red = lit where truth is dark; blue = dark where truth is lit. Linear visibility difference, scale 0..1; black = agreement / unlit.', font=small, fill='#bdc9db')
            note = 'Gold ring: light centre. Error compares production visibility against the oracle at the same pixel centres.'
            if light.kind == 'projector':
                note = 'Gold ring: light centre. Projector cone is applied to lighting, while cube visibility remains omnidirectional.'
            if name == '05_three_face_corner':
                # The exact micrometre measurements are also useful when the
                # subpixel seam is too small to distinguish in the screenshot.
                view = np.eye(4); view[:3, :3] = cam.view[:3, :3]
                values = Probe(r).run([marker-np.eye(3)[i]*1.e-6 for i in range(3)], light, near, view=view)*255
                note = 'Gold ring: three-face meeting on virtual receiver floor (sphere-only atlas). Probes 1 um apart (0..255): %s; spread %.2f.' % (', '.join('%.2f' % v for v in values), np.ptp(values))
            draw.text((gap, y+50), note, font=small, fill='#ffe19a')
            path = directory / (name+'.png'); sheet.save(path)
            print('[world-gl images] saved', path, flush=True)
    finally:
        r.old_packing = saved_packing
        r.gl.use(r.filter); r.gl.ui(debug_loc, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--glm-include'); ap.add_argument('--out', default='.tmp/relight_world_gl')
    ap.add_argument('--shader-ref',default='WORKTREE')
    ap.add_argument('--images', metavar='DIR', help='also save five labelled real-GL contact sheets (does not affect gate checks)')
    args = ap.parse_args()
    out = Path(args.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    WM.check(out / 'math')
    contracts = lifecycle_checks()
    host = build_host(out, args.glm_include)
    rows = []
    try:
        gl = G.GL()
        assert not any(x in gl.renderer.lower() for x in ('gdi generic','llvmpipe','softpipe','swiftshader','lavapipe','software','basic render')), 'FAIL: hardware GPU unavailable'
        r = RealRunner(G.SourceTree(args.shader_ref), host, gl=gl)
        print('[world-gl] GPU:', r.gl.renderer, flush=True)
        raster = Raster(r)
        reference = None
        for name, kw in [('empty_floor', dict(capsule=False)), ('capsule', {}),
                         ('rotated_single', dict(emitter='back')), ('rotated_double', dict(emitter='none')),
                         ('rotated_projector', dict(emitter='none', projector=True)),
                         ('translated_active', dict(emitter='none', translated=True))]:
            sc, light = fixture(**kw)
            v, mask, dark = evaluate(r, raster, sc, light)
            ok = dark < .1 if not kw.get('capsule', True) else 1 < dark < 30
            if name == 'rotated_double': reference = v
            if name == 'rotated_projector': ok &= np.array_equal(v, reference)
            rows.append(dict(name=name, dark_percent=dark, passed=bool(ok)))
            print('[world-gl]', rows[-1], flush=True)
        # World Shadow pick: a PICKED projector that owns a native SL shadow map is resolved through the cube. The viewer uploads for it exactly a
        # world-owned channel (area_world 1, contact_fade 0, contact = the map's blind band / the mapped contact); the same projector UNPICKED keeps
        # "SL map + contact" (area_world 0, contact_fade = its slot's fade) and so takes nothing from the cube. Same projector, same camera.
        mapped_contact = .15
        sc, light = fixture(emitter='none', projector=True)
        v_native, mask, dark_native = evaluate(r, raster, sc, light, channel=dict(owned=False, contact_fade=1.0), contact=mapped_contact)
        v_picked, _, dark_picked = evaluate(r, raster, sc, light, channel=dict(owned=True, contact_fade=0.0), contact=mapped_contact)
        sc_pt, light_pt = fixture(emitter='none')
        v_point, _, dark_point = evaluate(r, raster, sc_pt, light_pt, contact=mapped_contact)
        rows.append(dict(name='picked_native_projector_resolves_through_cube', dark_percent=dark_picked, point_cube_dark_percent=dark_point,
                         passed=bool(1 < dark_picked < 30 and np.array_equal(v_picked, v_point))))
        rows.append(dict(name='unpicked_native_projector_keeps_native_path', dark_percent=dark_native, cube_dark_percent=dark_picked,
                         passed=bool(dark_native < .5 and dark_picked - dark_native > 1 and not np.array_equal(v_native, v_picked))))
        for row in rows[-2:]: print('[world-gl]', row, flush=True)
        for yaw, pitch in [(-54.462, 28.635), (0, 20), (45, 35), (90, 60), (135, 25), (225, 45)]:
            for capsule in (False, True):
                sc, light = fixture(capsule=capsule)
                _, _, dark = evaluate(r, raster, sc, light, yaw, pitch)
                rows.append(dict(name='sweep_%g_%g_%s' % (yaw, pitch, capsule), dark_percent=dark,
                                 passed=bool(dark < .1 if not capsule else dark > .1)))
        sc, light = fixture(capsule=False)
        r.old_packing = True
        _, _, dark = evaluate(r, raster, sc, light)
        rows.append(dict(name='old_packing_negative_control', dark_percent=dark, rejected=bool(dark >= .1), passed=bool(dark > 10)))
        r.old_packing = False
        probe = Probe(r)
        skins = skinned_caster_checks(r, probe)
        rows.extend(skins)
        for row in skins: print('[world-gl]', row, flush=True)
        first = len(rows)
        rows.extend(seam_checks(r,raster,probe))
        # Review round 5's exact 2 m reproduction (25/255 before round 8), identity and rotated camera, at the reviewed 1 and 2 m sources.
        for radius in (1., 2.):
            for vname, view in SEAM_VIEWS:
                v = codex_corner(r,raster,probe,radius,view)
                rows.append(dict(name='codex_corner_%gm_%s' % (radius,vname),values_255=[round(float(x),2) for x in v],delta_255=float(np.ptp(v)),
                                 passed=bool(np.ptp(v) <= SEAM_LIMIT)))
        rows.append(softbox_check(r,raster,probe,1.))
        rows.append(softbox_check(r,raster,probe,2.))
        # Grazing receivers on an empty floor (plane guard / cap), and a tile-bleed fixture (blocker only in the neighbouring tiles).
        for radius in (1., 2.):
            dark = grazing_value(r,raster,radius)
            rows.append(dict(name='grazing_%gm_empty_floor' % radius,dark_percent=dark,limit_percent=GRAZING_LIMIT,passed=bool(dark <= GRAZING_LIMIT)))
        lit = bleed_value(r,probe)
        rows.append(dict(name='tile_bleed_neighbour_blocker',min_visibility=lit,passed=bool(lit >= .99)))
        # CONTROLS: the pre-fix behaviours as shader-text mutations. Each MUST be rejected (its row passes only when the gate catches it).
        m, mraster, mprobe = mutant(r,MUT_HARD_FACE)
        v = codex_corner(m,mraster,mprobe,2.)
        rows.append(dict(name='control_hard_face_choice_rejected',delta_255=float(np.ptp(v)),passed=bool(np.ptp(v) > SEAM_LIMIT)))
        m, mraster, mprobe = mutant(r,MUT_NO_PLANE_GUARD)
        dark = grazing_value(m,mraster,2.)
        rows.append(dict(name='control_unguarded_plane_rejected',dark_percent=dark,passed=bool(dark > GRAZING_LIMIT)))
        m, mraster, mprobe = mutant(r,MUT_WIDE_UNCLAMPED)
        lit = bleed_value(m,mprobe)
        rows.append(dict(name='control_unclamped_tile_read_rejected',min_visibility=lit,passed=bool(lit < .9)))
        m, mraster, mprobe = mutant(r,MUT_WIDE_OVERLAP)
        lit = bleed_value(m,mprobe)
        rows.append(dict(name='control_tile_clamp_holds_with_wide_overlap',min_visibility=lit,passed=bool(lit >= .99)))
        rows.append(dict(name='control_planner_fallback_rejected',passed=bool(planner_control(out))))
        for row in rows[first:]: print('[world-gl]',row,flush=True)
        flicker = flicker_checks(r,raster,out)
        rows.extend(row for row in flicker if 'passed' in row)
        diagnostics = [row for row in flicker if 'passed' not in row]
        for row in flicker: print('[world-gl]',row,flush=True)
        rows.extend(contracts)
        result = dict(renderer=r.gl.renderer, rows=rows, diagnostics=diagnostics, passed=all(x['passed'] for x in rows))
        (out / 'results.json').write_text(json.dumps(result, indent=2))
        if args.images:
            save_images(r, raster, args.images)
        print('[world-gl] %s (%d checks); results: %s' % ('PASS' if result['passed'] else 'FAIL', len(rows), out / 'results.json'))
        return 0 if result['passed'] else 1
    finally:
        host.terminate(); host.wait()


if __name__ == '__main__':
    try: sys.exit(main())
    except Exception as exc:
        print('[world-gl] FAIL (gate could not run):', exc, file=sys.stderr)
        raise
