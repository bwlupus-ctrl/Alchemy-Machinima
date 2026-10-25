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
    # Soft shadow LOOK controls: the five settings reach the shaders through the one sanitizing helper, are scene-saved, and keep their defaults.
    assert 'ALAreaShadow::lookUniform(look_strength(),look_contrast(),look_world_amount(),look);' in pipe, 'look uniform packing lost'
    assert 'gAreaShadowFilterProgram.uniform3fv(LLStaticHashedString("area_look"),1,look)' in pipe, 'area_look upload lost'
    assert 'LLStaticHashedString("area_screen_reach"),world_any ? look_reach_m:0.f,look_screen)' in pipe, 'area_screen_reach upload lost'
    director = (REPO / 'indra/newview/llfloaterdirector.cpp').read_text()
    settings = (REPO / 'indra/newview/app_settings/settings.xml').read_text()
    for name, default in (('RenderAreaShadowStrength','1'),('RenderAreaShadowContrast','1'),('RenderAreaShadowWorldScreenReach','0'),
                          ('RenderAreaShadowWorldAmount','1'),('RenderAreaShadowScreenAmount','1')):
        assert '"%s",' % name in director, 'not scene-saved: ' + name
        m = re.search(r'<key>%s</key>\s*<map>.*?<key>Type</key><string>F32</string>\s*<key>Value</key><real>([^<]+)</real>' % name, settings, re.S)
        assert m and float(m.group(1)) == float(default), 'default drifted: ' + name
    return [dict(name=n,passed=True) for n in ('attachment_scope_integration','native_map_pin_release','picked_projector_pin_release_exemption',
            'complete_linkset_cache_invalidation','GL_teardown','snapshot_pin_preservation','production_softbox_radius',
            'production_packing_and_extents_wiring','production_atlas_copy_wiring','soft_shadow_look_wiring')]


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
    area_n_shade = area_n_geo;
    float visibility = probe_plane == 2 ?
        awFaceTap(0,4,normalize(vec3(1.2,0,1)),vec3(0,0,-1),-2.0,0.003,vec3(0,0,1),2.0,1.0,false).x : awVisibility(0);
    frag_color = vec4(vec3(visibility),1);
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
    """The worktree with named substitutions in one selected shader module: a CONTROL, never production text. A control is only
    meaningful if its target text still exists, so a vanished target is a gate failure."""
    def __init__(self, base, subs, filename="areaShadowFilterF.glsl"):
        super().__init__(base.ref)
        self.subs = subs
        self.filename = filename

    def read(self, path):
        text = super().read(path)
        if Path(path).name == self.filename:
            for old, new in self.subs:
                assert old in text, 'mutation target vanished from the shader: ' + old
                text = text.replace(old, new)
        return text


def mutant(r, subs, filename="areaShadowFilterF.glsl"):
    """(runner, raster, probe) on the mutated shader text, sharing the GL context and host."""
    m = RealRunner(MutantTree(r.tree, subs, filename), r.host, gl=r.gl)
    return m, Raster(m), Probe(m)


# The pre-fix behaviours, as shader-text controls. Each MUST be rejected by the check named in main().
MUT_HARD_FACE = [('vec3 w = smoothstep(vec3(AW_OVERLAP*major),vec3(major),a);',
                  'vec3 w = vec3(a.x >= a.y && a.x >= a.z ? 1.0 : 0.0, (a.x < a.y && a.y >= a.z) ? 1.0 : 0.0, (a.x < a.z && a.y < a.z) ? 1.0 : 0.0);')]
MUT_NO_PLANE_GUARD = [('return vec4(lessThan((z+bias)*max(vec4(den+du,den+dr+du,den+dr,den),vec4(1.e-5)),vec4(receiver2)));',
                       'return vec4(1.0);')]
MUT_WIDE_OVERLAP = [('const float AW_OVERLAP = 0.95;', 'const float AW_OVERLAP = 0.5;')]
# COST upper bound only (relight_world_cost.py): every tap reads all three faces, as if every receiver ray sat on a cube corner.
MUT_ALL_FACES = [('const float AW_OVERLAP = 0.95;', 'const float AW_OVERLAP = 0.0;')]
MUT_TWO_FACES = [('vec3 w = smoothstep(vec3(AW_OVERLAP*major),vec3(major),a);', 'vec3 w = vec3(1.0,1.0,0.0);')]   # every tap reads two faces, as if every receiver ray sat on a cube edge
MUT_WIDE_UNCLAMPED = [('vec2 q = clamp(centre-0.5,vec2(0.0),vec2(AW_TILE-2.0));', 'vec2 q = centre-0.5;')]


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


def tile_clamp_value(r, probe):
    """Stress one tile's addressing beyond its gutter, without altering overlap.

    Only +Z is clear; its right-hand neighbour is a near blocker. The selected
    production face tap must remain inside +Z even for this out-of-tile ray.
    """
    light = S.Light('clamp', (0,0,0), .1, 10., seed=123)
    r.gpu_atlas = bleed_atlas(r, legit=(4,))
    return float(probe.run([[0,0,2]], light, .05, plane=2)[0])


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
    reach_configs = {'cube_reach%g_steps%d' % (reach, steps): (reach, steps)
                     for reach in (2.,8.) for steps in (32,64)}
    seq = {cfg: [] for cfg in ('cube','cube+thin',*reach_configs,'cube_worldonly')}
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
            look = dict()
            if cfg in reach_configs:
                reach, steps = reach_configs[cfg]
                plan = dict(plan, steps=steps)
                look = dict(reach=reach)
            v = r.run(cam,depth,norm,[light],plan,world=world,look=look)
            if cfg in reach_configs:
                repeated = r.run(cam,depth,norm,[light],plan,world=world,look=look)
                assert np.array_equal(v,repeated), 'reach sampling crawls at an unchanged camera'
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
        # contact-disabled path diagnoses aliasing; reach at 30 degree jumps is
        # diagnostic too (the frame-motion gate below keeps the same limits).
        if cfg not in reach_configs and cfg != 'cube_worldonly': row['passed'] = worst<=.06 and pop<=.05
        if cfg in reach_configs: row['camera_jump_degrees'] = 30
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


def reach_motion_checks(r, raster, out):
    """Frame-sized 0.1 degree camera motion at all 12 azimuths of B's low orbit.

    Reproject the same visible world probes. Keep the existing p95/pop limits;
    the separate 30 degree orbit diagnostics measure large viewpoint changes.
    No temporal history: an unchanged camera must reproduce every byte too.
    """
    sc = S.scene_sphere()
    poses = [p for p in sc.poses if p.group == 'camorbit_low']
    light = sc.light_sets[poses[0].cfg][0]
    near = CS.emitter_near(sc,light)
    r.gpu_atlas = raster.atlas(sc,light,near)
    configs = [(reach,steps) for reach in (2.,8.) for steps in (32,64)]
    measured = {cfg: [] for cfg in configs}
    pts = sc.probes
    gt = S.trace_light(sc,light,pts,np.tile([0.,0.,1.],(len(pts),1)),np.arange(len(pts),dtype=np.uint32),S=256)
    gt = (1.-gt['nblk'].astype(np.float32)/256)[None,:]
    world = {'atlas': np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),'channels':
             [dict(row=0,near=near,far=light.range,r_map=light.radius,contact=CS.contact_distance(light.range),seed=light.seed)]}
    deterministic = True
    a = np.radians(.1)
    rotate = np.array([[np.cos(a),-np.sin(a),0.],[np.sin(a),np.cos(a),0.],[0.,0.,1.]])
    for pose in poses:
        seq = {cfg: [] for cfg in configs}
        centre = np.array(pose.target)
        for eye in (pose.eye, centre + rotate @ (np.array(pose.eye)-centre)):
            cam = S.Camera(eye,pose.target,1920,1080,pose.vfov)
            g = S.cast_primary(sc,cam)
            assert T.blocker_info(sc,cam,g)['sphere']['status'] == 'on', 'motion fixture lost the on-screen blocker'
            _,depth,norm = S.encode_gbuffer(cam,g)
            xy = T.probe_projection(sc,cam,g)
            for reach,steps in configs:
                plan = dict(samples=8,steps=steps,half=True,near_steps=0)
                look = dict(reach=reach)
                v = r.run(cam,depth,norm,[light],plan,world=world,look=look)
                repeated = r.run(cam,depth,norm,[light],plan,world=world,look=look)
                deterministic &= np.array_equal(v,repeated)
                _,pv = R.sample_full(v,(np.array([0]),np.array([0])),xy)
                seq[(reach,steps)].append(dict(probe=pv,cls=np.zeros_like(pv,dtype=np.uint8),cfg=0))
        for cfg in configs:
            pair = M.temporal(seq[cfg],[True,True],{0:gt})[0]
            pair['azimuth'] = pose.id
            measured[cfg].append(pair)
    rows = [dict(name='look_reach_motion_no_static_crawl',passed=bool(deterministic))]
    for (reach,steps),pairs in measured.items():
        (out / ('reach_motion_%g_steps%d.json' % (reach,steps))).write_text(json.dumps(pairs,indent=2))
        active = [p for p in pairs if 'p95' in p]
        worst = max(p['p95'] for p in active); pop = max(p['pop'] for p in active)
        rows.append(dict(name='look_reach%g_steps%d_reprojected_frame_motion' % (reach,steps),
                         p95=worst,pop=pop,pairs=len(active),camera_step_degrees=.1,
                         passed=bool(len(active) >= 6 and worst <= .06 and pop <= .05)))
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


# ---------------------------------------------------------------------------------------------------------------- soft shadow LOOK
# Shadow strength / Shadow contrast / Screen-space detail (reach) / World shadow amount / Screen-space amount, through the PRODUCTION gather
# and filter programs on the real raster. The reach gates use a "blind" cube (the atlas is rasterised WITHOUT the capsule while the G-buffer
# has it: a blocker the depth buffer sees and the cube does not), so only the screen-space reach can darken the floor behind it.
LOOK_ONE = 1.01 / 255       # one 8-bit step plus rounding slack
LOOK_CURVE = 1.6 / 255      # a slope-2 curve of an already 8-bit value: 2 * 0.5/255 + 0.5/255 plus slack
_LOOK_GBUFFERS = {}
LOOK_BASE = '18a07a3aca5'   # the commit before the LOOK controls: its shaders are the byte reference at the defaults


class PreLookTree(G.SourceTree):
    """Historical LOOK-free code with frozen 77348de6593 world functions.

    Never substitute the algorithm under test into a historical byte oracle.
    The optional outer reference retains the EV/detail historical lighting code.
    """
    def __init__(self, current, reference=LOOK_BASE):
        super().__init__(reference)
        self.world = G.SourceTree('77348de6593')

    def read(self, path):
        text = super().read(path)
        if Path(path).name == 'areaShadowFilterF.glsl':
            current = self.world.read(path)
            start = '#if __VERSION__ >= 400'
            text = text[:text.index(start)] + current[current.index(start):current.index('void main()')] + text[text.index('void main()'):]
            # The outer test isolates LOOK/EV/detail changes from the receiver
            # reconstruction deliberately changed by FIX2. Its entire world
            # oracle, including normal reconstruction, stays historical.
            text = text.replace('area_n_geo = awGeoNormal(texel);', '')
        return text


def historical_protected_mask(reference, eligible, radius=8):
    """Historical lit/umbra interiors, selected without the candidate output.

    Phase, horizon, receiver reconstruction and behind-surface corrections
    intentionally change penumbrae and silhouettes. Preserve byte protection
    on well-conditioned interiors AND gate those changed cases independently.
    """
    classes = np.where(reference == 0., 1, np.where(reference == 1., 2, 0))
    protected = (classes != 0) & eligible
    for axis in (0,1):
        for shift in range(-radius,radius+1):
            protected &= np.roll(classes,shift,axis) == classes
    protected[:radius,:] = protected[-radius:,:] = False
    protected[:,:radius] = protected[:,-radius:] = False
    return protected


def geometry_inside(prim, points):
    """Exact batched volume test; capsule entry roots omit inside origins."""
    if prim.kind == 'obb':
        return np.all(np.abs((points-np.asarray(prim.c))@prim.rot())<np.asarray(prim.h),axis=1)
    if prim.kind == 'sphere':
        delta=points-np.asarray(prim.c)
    else:
        a,b=np.asarray(prim.a),np.asarray(prim.b);axis=b-a
        t=np.clip(((points-a)@axis)/(axis@axis),0.,1.)
        delta=points-(a+t[:,None]*axis)
    return (delta*delta).sum(1)<prim.r*prim.r


def historical_geometry_mask(reference,scene,g,light,eligible,radius=8):
    """Historical interiors whose geometry agrees throughout filter support.

    Ignore the emitter just as independent disk truth does. The candidate is
    never consulted when selecting unchanged lit/umbra samples.
    """
    protected=historical_protected_mask(reference,eligible,radius)
    p=g['pos'][protected];normal=g['nrm'][protected]
    delta=np.asarray(light.pos)-p;distance=np.linalg.norm(delta,axis=1)
    nl=(normal*delta).sum(1)/np.maximum(distance,1.e-9)
    axis=np.zeros_like(normal);axis[:,2]=1.;axis[np.abs(normal[:,2])>.9]=(0,1,0)
    tangent=np.cross(normal,axis);tangent/=np.maximum(np.linalg.norm(tangent,axis=1,keepdims=True),1.e-9)
    bitangent=np.cross(normal,tangent)
    extent=4.*np.max(np.abs(delta),axis=1)/CS.USABLE/np.maximum(nl,.2)
    expected=reference[protected]==1.;stable=nl>.2
    ignored=set(scene.prim_indices(light.ignore))
    prims=[prim for i,prim in enumerate(scene.prims) if i not in ignored]
    for x,y in ((0,0),(-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1)):
        q=p+extent[:,None]*(x*tangent+y*bitangent)+normal*.0005
        ray=np.asarray(light.pos)-q;distance=np.linalg.norm(ray,axis=1);ray/=distance[:,None]
        clear=np.ones(len(q),bool)
        for prim in prims:clear &= (S.isect(prim,q,ray)>=distance)&~geometry_inside(prim,q)
        stable &= clear==expected
    protected[protected]=stable
    return protected


def look_frame(r, raster, light, capsule_in_gbuffer=True, capsule_in_cube=True, look=dict(), owned=True, size=(640, 360), yaw=-54.462, pitch=28.635,
               steps=12, stats=None):
    """One resolved frame. Returns (visibility (H, W), floor mask, G-buffer). Uploads exactly what renderAreaShadows would (the contact band
    is the production CS.contact_distance). look=dict() (the default) uploads every LOOK uniform at its default; look=None uploads NONE of them,
    which is only a program that was never given them while no earlier frame uploaded any (the programs keep their uniforms)."""
    t, p = np.radians([yaw, pitch])
    cam = S.Camera((6*np.cos(p)*np.cos(t), 6*np.cos(p)*np.sin(t), 6*np.sin(p)), (0, 0, .4), size[0], size[1])
    full, _ = fixture(capsule=True)
    cube_scene, _ = fixture(capsule=capsule_in_cube)
    gbuf_scene = full if capsule_in_gbuffer else fixture(capsule=False)[0]
    key = (capsule_in_gbuffer, size, yaw, pitch)
    if key not in _LOOK_GBUFFERS:
        g = S.cast_primary(gbuf_scene, cam)
        _, d32, n16 = S.encode_gbuffer(cam, g)
        _LOOK_GBUFFERS[key] = (g, d32, n16)
    g, depth, norm = _LOOK_GBUFFERS[key]
    near = CS.emitter_near(cube_scene, light)
    r.gpu_atlas = raster.atlas(cube_scene, light, near)
    world = {'atlas': np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), 'channels':
             [dict(row=0, near=near, far=light.range, r_map=min(light.radius, 2.), contact=CS.contact_distance(light.range), seed=light.seed,
                   owned=owned)]}
    plan = dict(samples=8, steps=steps, half=True, near_steps=0)
    v = r.run(cam, depth, norm, [light], plan, world=world if owned else None, look=look, stats=stats)[0]
    mask = (g['surf'] == 0) & (np.linalg.norm(g['pos'][..., :2], axis=2) < 8.)
    assert np.count_nonzero(mask) > 1000, 'fixture has insufficient floor coverage'
    return v, mask, g


def dark_percent(v, mask): return 100 * float(np.mean(v[mask] < .5))


def reach_growth_checks(r, raster):
    _, light = fixture()
    rows = []
    for reach in (2.,8.):
        images = []
        for steps in (32,64):
            v0, mask, _ = look_frame(r,raster,light,capsule_in_cube=False,steps=steps)
            v, _, _ = look_frame(r,raster,light,capsule_in_cube=False,steps=steps,look=dict(reach=reach))
            off, _, _ = look_frame(r,raster,light,capsule_in_cube=False,steps=steps,look=dict(reach=reach,screen_amount=0.))
            rows.append(dict(name='look_reach%g_steps%d_adds_darkness_keeps_contact' % (reach,steps),
                             passed=bool(np.all(v <= v0 + 1.e-6) and np.array_equal(off,v0) and np.sum((v0-v)[mask]) > 100.),
                             extra_depth=float(np.sum((v0-v)[mask]))))
            images.append(v)
        changed = int(np.count_nonzero(np.abs(images[0]-images[1])[mask] > LOOK_ONE))
        rows.append(dict(name='look_reach%g_32_to_64_changes_sampling' % reach, passed=changed >= 100, changed_pixels=changed))
    return rows


def reach_regression(r, raster):
    _, light = fixture()
    old, old_raster, _ = mutant(r, [('if (minimum_distance > 0.0 && hit_t <= minimum_distance) continue;', '')], 'screenSpaceRayUtil.glsl')
    args = dict(capsule_in_cube=False,look=dict(reach=4.,world_amount=0.))
    fixed, mask, _ = look_frame(r,raster,light,**args)
    stopped, _, _ = look_frame(old,old_raster,light,**args)
    x,y = 401,125  # reviewed floor pixel: the old march stops on an in-band candidate
    return dict(name='look_reach_searches_past_rejected_in_band_hit',
                passed=bool(mask[y,x] and abs(stopped[y,x]-154/255) < 1.e-6 and abs(fixed[y,x]-58/255) < 1.e-6),
                pixel=[int(x),int(y)], before=float(stopped[y,x]), after=float(fixed[y,x]))


def look_checks(r, raster, mut_runner=None, mut_raster=None):
    rows = []
    _, light = fixture()
    def row(name, ok, **kw): rows.append(dict(name=name, passed=bool(ok), **kw))
    # 0. Defaults: every LOOK uniform uploaded at its default is byte-identical to a program that was never given them.
    v_none, mask, g = look_frame(r, raster, light, look=None)         # the very first look frame: the programs were never given a LOOK uniform
    v_def, _, _ = look_frame(r, raster, light, look=dict())
    row('look_defaults_byte_identical_world_channel', np.array_equal(v_none, v_def), dark_percent=dark_percent(v_none, mask))
    v_none_s, _, _ = look_frame(r, raster, light, owned=False, look=dict())
    v_def_s, _, _ = look_frame(r, raster, light, owned=False, look=dict(strength=1., contrast=1., world_amount=1., reach=0., screen_amount=1.))
    row('look_defaults_byte_identical_unmapped_channel', np.array_equal(v_none_s, v_def_s), dark_percent=dark_percent(v_none_s, mask))
    # 0b. ... and byte-identical to the shaders of the commit before the LOOK controls existed (LOOK_BASE), on the same frames.
    base = RealRunner(PreLookTree(r.tree), r.host, gl=r.gl)
    base_raster = Raster(base)
    frozen = RealRunner(PreLookTree(r.tree, r.tree.ref), r.host, gl=r.gl)
    frozen_raster = Raster(frozen)
    same = []
    protected_frames = []
    for owned in (True, False):
        for in_cube in (True, False):
            a, _, _ = look_frame(frozen, frozen_raster, light, owned=owned, capsule_in_cube=in_cube)
            b, _, _ = look_frame(base, base_raster, light, owned=owned, capsule_in_cube=in_cube, look=None)
            same.append(bool(np.array_equal(a, b)))
            actual, floor, g = look_frame(r,raster,light,owned=owned,capsule_in_cube=in_cube)
            ray = np.asarray(light.pos)-g['pos']
            nl = (ray*g['nrm']).sum(2)/np.maximum(np.linalg.norm(ray,axis=2),1.e-9)
            protected = historical_protected_mask(b,floor & (nl>.2))
            protected_frames.append(dict(owned=owned,in_cube=in_cube,pixels=int(protected.sum()),
                                         shadow_pixels=int((protected & (b==0)).sum()),
                                         passed=bool(protected.sum()>1000 and np.array_equal(actual[protected],b[protected]))))
    row('look_defaults_byte_identical_to_pre_look_shaders', all(same), frames=same, base=LOOK_BASE,
        world_reference='77348de6593', scope='LOOK wrappers with historical world functions; production receiver separately gated')
    row('production_world_unchanged_interiors_match_77348de6593',all(x['passed'] for x in protected_frames),frames=protected_frames)
    # 1. Strength: v' = mix(1, v, s). 0.5 halves the shadow depth; 0 is no soft shadow; unmapped lights are scaled too.
    for owned, base, tag in ((True, v_none, 'world'), (False, v_none_s, 'unmapped')):
        v_half, _, _ = look_frame(r, raster, light, owned=owned, look=dict(strength=.5))
        depth_ratio = float(np.sum(1 - v_half[mask]) / max(np.sum(1 - base[mask]), 1.e-9))
        err = float(np.max(np.abs(v_half - (1 + base) / 2)))
        row('look_strength_half_halves_depth_' + tag, abs(depth_ratio - .5) <= .01 and err <= LOOK_ONE, depth_ratio=depth_ratio, max_err=err)
        v_zero, _, _ = look_frame(r, raster, light, owned=owned, look=dict(strength=0.))
        row('look_strength_zero_no_shadow_' + tag, bool(np.all(v_zero == 1.)), min_visibility=float(v_zero.min()))
    # 2. Contrast: v'' = pow(v, c). 2 darkens the penumbra midpoint, 0.5 gentles it; umbra and lit side stay.
    v_c2, _, _ = look_frame(r, raster, light, look=dict(contrast=2.))
    v_c05, _, _ = look_frame(r, raster, light, look=dict(contrast=.5))
    # pow(v, 2) has slope <= 2 and pow(v, .5) is checked in the inverse domain (its slope is unbounded at 0): budget 1.5/255 + slack.
    err2 = float(np.max(np.abs(v_c2 - v_none ** 2)))
    err05 = float(np.max(np.abs(v_c05 ** 2 - v_none)))
    mid = mask & (v_none >= .3) & (v_none <= .7)
    row('look_contrast_matches_pow_and_darkens_midpoint',
        err2 <= LOOK_CURVE and err05 <= LOOK_CURVE and np.count_nonzero(mid) >= 20 and float(np.mean(v_c2[mid])) < float(np.mean(v_none[mid])) - .05 and
        float(np.mean(v_c05[mid])) > float(np.mean(v_none[mid])) + .05 and float(np.max(v_c2[v_none == 0])) <= LOOK_CURVE and float(np.min(v_c2[v_none == 1])) >= 1 - LOOK_CURVE,
        max_err_c2=err2, max_err_c05=err05, midpoint_pixels=int(np.count_nonzero(mid)),
        mid_mean=dict(c1=float(np.mean(v_none[mid])), c2=float(np.mean(v_c2[mid])), c05=float(np.mean(v_c05[mid]))))
    v_both, _, _ = look_frame(r, raster, light, look=dict(contrast=2., strength=.5))
    err_both = float(np.max(np.abs(v_both - (1 + v_none ** 2) / 2)))
    row('look_contrast_then_strength_order', err_both <= LOOK_CURVE, max_err=err_both)
    # 3. World amount: world' = mix(1, world, a) BEFORE min(); 0.5 halves the WORLD depth only; 0 leaves just the contact band.
    v_w0, _, _ = look_frame(r, raster, light, look=dict(world_amount=0.))
    v_wh, _, _ = look_frame(r, raster, light, look=dict(world_amount=.5))
    contact_lit = (v_w0 >= 1 - 1.e-6) & (v_none < .9)           # the contact band is clear here: the world term alone decides
    err_w = float(np.max(np.abs(v_wh[contact_lit] - (1 + v_none[contact_lit]) / 2))) if np.any(contact_lit) else 9.
    dark_full, dark_w0 = dark_percent(v_none, mask), dark_percent(v_w0, mask)
    row('look_world_amount_zero_keeps_contact_band', 0 < dark_w0 < .3 * dark_full and bool(np.all(v_w0 >= v_none - 1.e-6)),
        dark_percent_world_amount_0=dark_w0, dark_percent_full=dark_full)
    row('look_world_amount_half_halves_world_depth_only', np.count_nonzero(contact_lit) > 100 and err_w <= LOOK_ONE and bool(np.all(v_wh >= v_none - 1.e-6)),
        pixels=int(np.count_nonzero(contact_lit)), max_err=err_w)
    # 4. Screen-space detail (reach) on a blind cube: the floor behind the capsule is lit by the cube, only the reach can darken it.
    kw = dict(capsule_in_cube=False)
    v0, _, _ = look_frame(r, raster, light, **kw)
    v0e, _, _ = look_frame(r, raster, light, look=dict(reach=0.), **kw)
    row('look_reach_zero_byte_identical_blind_cube', np.array_equal(v0, v0e), dark_percent=dark_percent(v0, mask))
    results = {}
    for reach in (1., 2., 4., 8.):
        results[reach], _, _ = look_frame(r, raster, light, look=dict(reach=reach), **kw)
    dark = {k: dark_percent(v, mask) for k, v in results.items()}
    row('look_reach_darkens_corner_fixture_that_reach_zero_leaves_lit', dark[4.] > dark_percent(v0, mask) + 1. and dark_percent(v0, mask) < .5,
        dark_percent_by_reach=dict([('0', dark_percent(v0, mask))] + [(str(k), v) for k, v in dark.items()]))
    row('look_reach_only_adds_darkness', all(bool(np.all(v <= v0 + 1.e-6)) for v in results.values()),
        worst_brightening=max(float(np.max(v - v0)) for v in results.values()))
    # Pure screen-space reference: the same blocker traced by the unmapped (full screen) path. Reach 8 should approach its look.
    v_screen, _, _ = look_frame(r, raster, light, owned=False, capsule_in_cube=False)
    dark_screen = dark_percent(v_screen, mask)
    row('look_reach_8_approaches_pure_screen_space', abs(dark[8.] - dark_screen) <= max(1.5, .35 * dark_screen),
        dark_percent_reach8=dark[8.], dark_percent_pure_screen=dark_screen)
    # Screen amount: 0 + reach 4 == reach 0 exactly (the contact band is untouched); 0.5 adds about half the extra darkness.
    v_s0, _, _ = look_frame(r, raster, light, look=dict(reach=4., screen_amount=0.), **kw)
    v_sh, _, _ = look_frame(r, raster, light, look=dict(reach=4., screen_amount=.5), **kw)
    extra_full = float(np.sum((v0 - results[4.])[mask])); extra_half = float(np.sum((v0 - v_sh)[mask]))
    row('look_screen_amount_zero_equals_reach_zero', np.array_equal(v_s0, v0))
    row('look_screen_amount_half_adds_half_the_darkness', extra_full > 0 and .35 <= extra_half / extra_full <= .65 and bool(np.all(v_sh <= v0 + 1.e-6)) and bool(np.all(v_sh >= results[4.] - 1.e-6)),
        ratio=extra_half / max(extra_full, 1.e-9))
    # An EMPTY floor at the longest reach: no darker than the pure screen-space path (the old look, the same thickness and march) on the same
    # fixtures. (A 1 m source 5 cm above the floor puts half its disk under the floor, so the old look itself reads ~1 % dark there: the reach may
    # add nothing beyond what that look already does, up to the grazing gate's 0.25 percentage points (the contact band's finer 5 mm march and the reach march
    # both read the floor; the world path's own grazing gate runs separately with the reach off).)
    worst_reach, worst_screen, over = 0., 0., 0.
    for height in (.05, .2):
        lamp = S.Light('lamp', (0., 0., height), 1., 10., seed=123)
        for yaw, pitch in [(-54.462, 28.635), (0, 20), (45, 35), (90, 60), (135, 25), (225, 45)]:
            args = dict(capsule_in_gbuffer=False, capsule_in_cube=False, yaw=yaw, pitch=pitch)
            v_reach, empty_mask, _ = look_frame(r, raster, lamp, look=dict(reach=8.), **args)
            v_scr, _, _ = look_frame(r, raster, lamp, owned=False, **args)
            a, b = dark_percent(v_reach, empty_mask), dark_percent(v_scr, empty_mask)
            worst_reach, worst_screen, over = max(worst_reach, a), max(worst_screen, b), max(over, a - b)
    row('look_reach_8_empty_floor_no_darker_than_pure_screen_space', over <= GRAZING_LIMIT, limit_percent=GRAZING_LIMIT, worst_dark_percent_reach8=worst_reach,
        worst_dark_percent_pure_screen=worst_screen, worst_excess_percent=over)
    # Mix controls together: world 0 + screen 1 + reach 0 is the contact band alone; screen amount never removes the band.
    v_mix, _, _ = look_frame(r, raster, light, look=dict(world_amount=0., screen_amount=0., reach=4.))
    v_mix_ref, _, _ = look_frame(r, raster, light, look=dict(world_amount=0.))
    row('look_screen_amount_zero_keeps_contact_band_with_world_off', np.array_equal(v_mix, v_mix_ref) and dark_percent(v_mix, mask) > 0,
        dark_percent=dark_percent(v_mix, mask))
    # CONTROL: a build that ignores Shadow strength must be rejected by the strength gate.
    if mut_runner is not None:
        v_mut, _, _ = look_frame(mut_runner, mut_raster, light, look=dict(strength=.5))
        ratio = float(np.sum(1 - v_mut[mask]) / max(np.sum(1 - v_none[mask]), 1.e-9))
        row('control_ignored_strength_rejected', abs(ratio - .5) > .05, depth_ratio=ratio)
    rows.extend(reach_growth_checks(r,raster))
    rows.append(reach_regression(r,raster))
    frozen, frozen_raster, _ = mutant(r, [('int reach_steps = min(area_steps,12)+int(ceil(float(max(area_steps-12,0))*clamp(reach_limit*0.125,0.0,1.0)));',
                                         'int reach_steps = min(area_steps,12);')], 'areaShadowF.glsl')
    frozen_rows = reach_growth_checks(frozen,frozen_raster)
    growth = [row for row in frozen_rows if '32_to_64' in row['name']]
    row('control_frozen_reach_steps_rejected', len(growth) == 2 and all(not item['passed'] for item in growth), rejected=growth)
    look_frame(r, raster, light)       # leave the programs holding the identity (default) uniforms for the rest of the gate
    return rows


def look_cost(r, raster, repeats=5):
    """GPU milliseconds of the gather at 1080p (half-resolution trace), the blind-cube fixture (nothing hits, so every ray marches its full
    steps: the worst case for the extra march). Diagnostic only."""
    _, light = fixture()
    out = {}
    for steps in (12, 32, 64):
        for reach in (0., 2., 8.):
            look_frame(r, raster, light, capsule_in_cube=False, look=dict(reach=reach), size=(1920, 1080), steps=steps)     # warm-up
            samples = []
            for _ in range(repeats):
                stats = {}
                look_frame(r, raster, light, capsule_in_cube=False, look=dict(reach=reach), size=(1920, 1080), steps=steps, stats=stats)
                samples.append(stats['gather_ms'])
            out['steps%d_reach%g_gather_ms' % (steps, reach)] = round(float(np.median(samples)), 3)
    look_frame(r, raster, light)
    return dict(name='look_cost_1080p_gather_ms', **out)


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
        # Soft shadow LOOK controls (strength, contrast, reach, World / Screen amounts), plus the control that a build ignoring Shadow strength is rejected.
        mut_r, mut_raster, _ = mutant(r, [('if (area_look.x != 0.0) frag_color = mix(vec4(1.0),frag_color,1.0-area_look.x);', '')])
        look_rows = look_checks(r, raster, mut_r, mut_raster)
        rows.extend(look_rows)
        for row in look_rows: print('[world-gl]', row, flush=True)
        look_diagnostic = look_cost(r, raster)
        print('[world-gl]', look_diagnostic, flush=True)
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
        lit = tile_clamp_value(m,mprobe)
        rows.append(dict(name='control_unclamped_tile_read_rejected',min_visibility=lit,passed=bool(lit < .9)))
        m, mraster, mprobe = mutant(r,MUT_WIDE_OVERLAP)
        lit = bleed_value(m,mprobe)
        rows.append(dict(name='control_tile_clamp_holds_with_wide_overlap',min_visibility=lit,passed=bool(lit >= .99)))
        rows.append(dict(name='control_planner_fallback_rejected',passed=bool(planner_control(out))))
        for row in rows[first:]: print('[world-gl]',row,flush=True)
        flicker = flicker_checks(r,raster,out)
        rows.extend(row for row in flicker if 'passed' in row)
        diagnostics = [row for row in flicker if 'passed' not in row]
        diagnostics.append(look_diagnostic)
        for row in flicker: print('[world-gl]',row,flush=True)
        motion = reach_motion_checks(r,raster,out)
        rows.extend(motion)
        for row in motion: print('[world-gl]',row,flush=True)
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
