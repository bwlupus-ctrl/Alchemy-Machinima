"""Hidden WGL compilation/render regression for production Studio clone shaders.

No viewer build. Uses relight_3d_gl's real driver/context, the complete production
vertex/fragment sources and production skin modules. External atmosphere and
indexed sampler plumbing are identity fixtures (not a viewer pipeline test).
Parity uses a front ellipsoid; animation uses a metre-scale avatar body,
cutout alpha hair cards and a rigid attachment with held palettes/transforms.
These synthetic fixtures cannot validate every imported asset or the viewer's
collector. Writes metrics and PNGs; unavailable OpenGL is a failure.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image, ImageDraw

import relight_3d_gl as G

ROOT = Path(__file__).resolve().parents[2]
SH = G.SHADERS + '/class1/'
F = SH + 'interface/actorghostF.glsl'
V = SH + 'interface/actorghostV.glsl'
NAMES = dict(zip((64,65,66,67,69,70,71,73,74,75,76,77),
                 ('Glass','Mercury','Marble','Crystal','Magma','Void','Sumi-e',
                  'Lidar','Code Rain','Datamosh','Scan Build','Contour')))


class Runner:
    def __init__(self, width, height):
        self.gl = g = G.GL()
        self.definitions = {}
        self.width, self.height = width, height
        self.scene_copies = 0
        self.projection_x = .7 * height / width
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0):
            g.disable(cap)
        self.clear = g.fn('glClearBufferfv', None, g.U, g.I, C.POINTER(g.F))
        g.enable = g.fn('glEnable', None, g.U)
        self.disable_attr = g.fn('glDisableVertexAttribArray', None, g.U)
        self.attr4 = g.fn('glVertexAttrib4f', None, g.U, g.F, g.F, g.F, g.F)
        self.attri = g.fn('glVertexAttribI1i', None, g.U, g.I)
        self.mat3 = g.fn('glUniformMatrix3fv', None, g.I, g.I, g.U, C.POINTER(g.F))
        self.mat34 = g.fn('glUniformMatrix3x4fv', None, g.I, g.I, g.U, C.POINTER(g.F))
        self.fbo, self.vbo = g.U(), g.U()
        g.gen_fbo(1, C.byref(self.fbo))
        g.bind_fbo(0x8D40, self.fbo)
        self.target = self.texture(None, width, height, 0x8814)
        g.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, self.target, 0)
        depth = g.U()
        g.gen_tex(1,C.byref(depth));g.bind_tex(0x0DE1,depth)
        g.teximage(0x0DE1,0,0x8CAC,width,height,0,0x1902,0x1406,None)
        g.fbo_tex(0x8D40,0x8D00,0x0DE1,depth,0)
        assert g.fbo_status(0x8D40) == 0x8CD5
        self.scene = self.texture(None, width, height, 0x8058, 'linear')
        y, x = np.mgrid[:height, :width]
        check = ((x // max(1, width//24) + y // max(1, height//24)) % 2).astype(np.float32)
        self.background = np.stack((.08+.65*check, .15+.5*x/width,
                                    .12+.6*y/height, np.ones_like(check)), -1).astype(np.float32)
        # Nearest-only poison fixture: whole transparent taps must contribute
        # no RGB. Filtered contamination is measured separately below.
        y, x = np.mgrid[:128, :128] / 127.0
        tex = np.stack((.25 + .65*x, .2 + .6*y, .3 + .2*np.sin(x*30),
                        np.where((x > .44) & (x < .56) & (y > .25) & (y < .75), 0., 1.)), -1).astype(np.float32)
        tex[tex[..., 3] == 0, :3] = 0
        self.source_pixels = tex.copy()
        self.diffuse = self.texture(tex, 128, 128, 0x8814)
        alternate = tex.copy()
        alternate[alternate[...,3] == 0, :3] = (19., 7., 31.)
        self.alternate = self.texture(alternate, 128, 128, 0x8814)
        # Tessellated front hemisphere, no overlapping/back faces or depth fixture.
        verts = []
        def point(i, j):
            a, b = np.pi*i/96, np.pi + np.pi*j/192
            n = np.array((np.sin(a)*np.cos(b), np.sin(a)*np.sin(b), np.cos(a)))
            p = n*np.array((.52, .35, 1.08))
            normal = n/np.array((.52, .35, 1.08))
            normal /= np.linalg.norm(normal)
            return [*p, *normal, j/192, i/96]
        for i in range(96):
            for j in range(192):
                for a, b in ((i,j),(i+1,j),(i,j+1),(i,j+1),(i+1,j),(i+1,j+1)):
                    verts.append(point(a,b))
        self.verts = np.array(verts, np.float32)
        # Adjacent scalar slots on triangle boundaries, as Studio slot replay.
        self.slot_vbo = g.U()
        g.gen_buf(1, C.byref(self.slot_vbo))
        g.bind_buf(0x8892, self.slot_vbo)
        slots = np.repeat((self.verts.reshape(-1,3,8)[...,0].mean(axis=1) > 0).astype(np.int32), 3)
        g.buffer_data(0x8892, slots.nbytes, slots.ctypes.data, 0x88E4)
        g.gen_buf(1, C.byref(self.vbo))
        g.bind_buf(0x8892, self.vbo)
        g.buffer_data(0x8892, self.verts.nbytes, self.verts.ctypes.data, 0x88E4)

    def texture(self, data, w, h, fmt, filtering="nearest"):
        g = self.gl
        tex = g.U()
        g.gen_tex(1, C.byref(tex))
        g.bind_tex(0x0DE1, tex)
        g.teximage(0x0DE1, 0, fmt, w, h, 0, 0x1908, 0x1406,
                   None if data is None else data.ctypes.data)
        for key in (0x2800, 0x2801):
            g.param(0x0DE1, key, 0x2600 if filtering == "nearest" else 0x2601)
        for key in (0x2802, 0x2803):
            g.param(0x0DE1, key, 0x812F)
        if filtering == "mip":
            g.fn("glGenerateMipmap", None, g.U)(0x0DE1)
            g.param(0x0DE1, 0x2801, 0x2703)  # LINEAR_MIPMAP_LINEAR
        return tex.value

    def program(self, tree, defs=(), vertex=V, probe=False):
        g = self.gl
        header = '#version 420\n#define MAX_JOINTS_PER_MESH_OBJECT 110\n'
        header += ''.join('#define %s 1\n' % x for x in defs)
        header += '#define GHOST_INDEXED_CHANNELS 4\n'
        vs, fs = tree.read(vertex), tree.read(F)
        if 'HAS_SKIN' in defs:
            vs += tree.read(SH + 'avatar/objectSkinV.glsl')
        if vertex.endswith('avatarActorGhostV.glsl'):
            vs += tree.read(SH + 'avatar/avatarSkinV.glsl')
        if 'GHOST_WORLD_PASS' in defs:
            # Explicit external-function fixtures; production mains stay intact.
            vs += '\nvoid calcAtmospherics(vec3 p) {}\n'
            fs += '\nvec3 getAdditiveColor(){return vec3(0);}\nvec3 getAtmosAttenuation(){return vec3(1);}\nvec4 applySkyAndWaterFog(vec3 p,vec3 a,vec3 t,vec4 c){return c;}\n'
        if 'GHOST_INDEXED_WORLD' in defs:
            vs += '\nin int texture_index; flat out int vary_texture_index; void passTextureIndex(){vary_texture_index=texture_index;}\n'
            fs = 'flat in int vary_texture_index; uniform sampler2D diffuseMap; vec4 diffuseLookup(vec2 uv){return texture(diffuseMap,uv);}\n' + fs
        if probe:
            fs = 'in vec3 vary_object_position; out vec4 frag_color; void main(){frag_color=vec4(vary_object_position,1);}'
        pr = g.program()
        for stage, source in ((0x8B31, vs), (0x8B30, fs)):
            sh = g.create_shader(stage)
            raw = C.c_char_p((header + source).encode())
            g.source(sh, 1, C.byref(raw), None)
            g.compile(sh)
            ok = g.I()
            g.getshader(sh, 0x8B81, C.byref(ok))
            if not ok.value:
                log = C.create_string_buffer(64000)
                g.slog(sh, len(log), None, log)
                raise AssertionError(str(defs) + ' ' + str(stage) + ' ' + log.value.decode())
            g.attach(pr, sh)
            g.fn('glDeleteShader', None, g.U)(sh)
        g.link(pr)
        ok = g.I()
        g.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            g.plog(pr, len(log), None, log)
            raise AssertionError(str(defs) + log.value.decode())
        self.definitions[pr] = (defs, vertex)
        return pr

    def render(self, pr, look, time=2., shift=0., scale=1., distort=0, phase=.73, mask=True, tint=(1,1,1,1), opaque=False,
               shift_y=0., rotation=0., slot=-1, adjacent_slots=False,
               velocity=(0.,0.), scene=False, composite=False,
               occlude=False, keep=False, stale_scene=False, opacity=.85, attachment_rotation=0., scene_rect=False,
               fade=1., native_fade=False, depth_test=False, held_pose=None,
               mesh=None, palette=None, object_matrix=None, clone_scale=1., raw_time=False,
               effect_fps=0., params=(0.,1.,0.,8.),
               preserve_depth=False, prime=False, brightness=1.):
        g = self.gl
        defs, vertex = self.definitions[pr]
        if look == 76 and not raw_time:
            time += 6.0  # Mid-build plane/lattice, rather than the built endpoint.
        g.use(pr)
        loc = lambda name: g.loc(pr, name.encode())
        def floats(name, *v):
            g.uf[len(v)](loc(name), *v)
        for name, value in dict(ghostLook=look, ghostIsClone=0, ghostSlot=slot,
                                ghostDistort=distort, ghostUseVertexAlpha=1,
                                ghostAlphaCutoffMode=1,
                                ghostWorldLinear=int('GHOST_WORLD_PASS' in defs)).items():
            g.ui(loc(name), value)
        for name in ('color', 'ghostSystemColor'):
            floats(name, .42, .72, .94, opacity)
        floats('ghostFadeProgress', 1.-fade)
        g.ui(loc('actorFxEnabled'), 2 if native_fade and fade < 1 else 0)
        floats('actorFxDissolveProgress', 1.-fade)
        floats('sil_clone_progress', 1.-fade)
        floats('sil_cover', 1., 1.)
        g.ui(loc('sil_blended'), 0)
        g.ui(loc('mask_vertex_alpha'), 0)
        g.ui(loc('mask_indexed'), 0)
        g.ui(loc('mask_material_slots'), 0)
        g.ui(loc('mask_vec3_position'), 0)
        for i in range(4): g.ui(loc('mask_textures[%d]' % i), 0)
        for i in range(8): floats('mask_cutoff[%d]' % i, .5 if mask else -1.)
        floats('ghostTime', np.floor(time*effect_fps)/effect_fps if effect_fps else time)
        floats('ghostParams', *params)
        floats('ghostAux', .5 if mask else 0., 1., 0., phase)
        floats('ghostFx', 0., 0., 0., brightness)
        floats('ghostAlpha', 0. if opaque else 1., 1.)
        floats('ghostDistortParams', .4 if distort else 0., .5, .5, 0.)
        floats('ghostScreenSize', self.width, self.height)
        floats('ghostScreenOrigin', 0., 0.)
        floats('ghostVelocity', *velocity)
        g.ui(loc('ghostSceneValid'), int(scene))
        g.ui(loc('ghostSceneMap'), 1)
        floats('ghostDissolveProgress', .28 if look == 10 else 0.)
        floats('ghostCoverageLayerStrength', -1.)
        floats('alrParams', 0., 0., 1., 0.)
        # Indexed fixture arrays: same material contract on each slot.
        for i in range(4):
            floats('ghostIndexedMaterialFactor[%d]' % i, 1., 1., 1., 1.)
            floats('ghostIndexedTextureAlpha[%d]' % i, 1.)
            floats('ghostIndexedMinimumAlpha[%d]' % i, .5)
            g.ui(loc('ghostIndexedUseVertexAlpha[%d]' % i), 1)
            g.ui(loc('ghostIndexedAlphaCutoffMode[%d]' % i), 1)
        view = np.array([[1,0,0,shift],[0,0,1,0],[0,-1,0,-4],[0,0,0,1]],np.float32)
        pose = np.eye(4,dtype=np.float32)
        if held_pose is not None:
            x,y,angle = held_pose
            co,si=np.cos(angle),np.sin(angle)
            pose = np.array([[co,-si,0,x],[si,co,0,y],[0,0,1,0],[0,0,0,1]],np.float32)
            if 'HAS_SKIN' not in defs: view = view @ pose # held rigid attachment render matrix
        # Camera roll and zoom also exercise transformed pattern footprints.
        co, si = np.cos(rotation), np.sin(rotation)
        roll = np.array([[co,-si,0,0],[si,co,0,shift_y],[0,0,1,0],[0,0,0,1]], np.float32)
        view = roll @ view
        view = view @ np.diag([clone_scale,clone_scale,clone_scale,1.]).astype(np.float32)
        if object_matrix is not None: view = view @ object_matrix
        # Rotate the attachment's local axes independently of the camera/up.
        ac, asi = np.cos(attachment_rotation), np.sin(attachment_rotation)
        attach = np.array([[ac,0,asi,0],[0,1,0,0],[-asi,0,ac,0],[0,0,0,1]],np.float32)
        view = view @ attach
        # Orthographic camera allows exact integer-pixel reprojection tests.
        proj = np.diag([scale*self.projection_x, scale*.85, -.1, 1.]).astype(np.float32)
        for name, mat in (('modelview_matrix', view), ('projection_matrix', proj),
                          ('modelview_projection_matrix', proj@view), ('texture_matrix0', np.eye(4,dtype=np.float32))):
            a = np.ascontiguousarray(mat.T)
            g.matrix(loc(name), 1, 0, a.ctypes.data_as(C.POINTER(g.F)))
        a = np.ascontiguousarray(view[:3,:3].T)
        self.mat3(loc('normal_matrix'), 1, 0, a.ctypes.data_as(C.POINTER(g.F)))
        # Render HAS_SKIN with a real identity object palette.
        if vertex.endswith('avatarActorGhostV.glsl'):
            a = np.repeat(view[:3, None, :], 15, axis=1).copy()
            g.u4fv(loc('matrixPalette[0]'), 45, a.ctypes.data_as(C.POINTER(g.F)))
        elif 'HAS_SKIN' in defs:
            a = (pose if palette is None else pose @ palette)[:3].copy()
            self.mat34(loc('matrixPalette[0]'), 1, 0, a.ctypes.data_as(C.POINTER(g.F)))
        g.bind_buf(0x8892, self.vbo)
        draw_verts = self.verts if mesh is None else mesh
        g.buffer_data(0x8892, draw_verts.nbytes, draw_verts.ctypes.data, 0x88E4)
        used = []
        for name, count, offset in (('position',3,0),('normal',3,12),('texcoord0',2,24)):
            ix = g.attr_loc(pr,name.encode())
            if ix >= 0:
                g.enable_attr(ix); used.append(ix)
                g.attr_ptr(ix,count,0x1406,0,32,C.c_void_p(offset))
        for name, value in (('diffuse_color',tint),('weight4',(.25,.25,.25,.25)),('weight',(0,0,0,0))):
            ix = g.attr_loc(pr,name.encode())
            if ix >= 0:
                self.disable_attr(ix); self.attr4(ix,*value)
        ix = g.attr_loc(pr,b'texture_index')
        if ix >= 0:
            if adjacent_slots:
                g.bind_buf(0x8892, self.slot_vbo)
                g.enable_attr(ix); used.append(ix)
                g.fn('glVertexAttribIPointer',None,g.U,g.I,g.U,g.I,C.c_void_p)(ix,1,0x1404,4,None)
            else:
                self.disable_attr(ix); self.attri(ix,0)
        g.active(0x84C0); g.bind_tex(0x0DE1,self.diffuse); g.ui(loc('diffuseMap'),0)
        g.bind_fbo(0x8D40,self.fbo); g.viewport(0,0,self.width,self.height)
        depth_mask = g.fn('glDepthMask',None,g.U)
        if depth_test:
            g.enable(0x0B71)
            g.fn('glDepthFunc',None,g.U)(0x0203)  # production LEQUAL
            depth_mask(1)
            if not preserve_depth: self.clear(0x1801,0,(g.F*1)(0. if occlude else 1.))
            if preserve_depth and not prime: depth_mask(0)
        else: g.disable(0x0B71)
        if not keep: self.clear(0x1800,0,(g.F*4)(0,0,0,0))
        if not keep and (composite or scene):
            g.bind_tex(0x0DE1, self.target)
            g.fn('glTexSubImage2D',None,g.U,g.I,g.I,g.I,g.I,g.I,g.U,g.U,C.c_void_p)(
                0x0DE1,0,0,0,self.width,self.height,0x1908,0x1406,self.background.ctypes.data)
            g.bind_tex(0x0DE1, self.diffuse)
        if scene and not stale_scene:
            # Each Glass copies the already composited DRAW target immediately
            # before drawing; keep=True retains every farther Studio clone.
            assert self.scene != self.target
            g.active(0x84C1); g.bind_tex(0x0DE1,self.scene)
            rect = [0,0,self.width,self.height]
            if scene_rect:
                # Independent GL coverage check for a padded sub-rectangle.
                # Production sphere/cube projection is executed in the C++ fixture.
                points=np.c_[self.verts[:,:3],np.ones(len(self.verts))] @ (proj@view).T
                xy=(points[:,:2]/points[:,3:4]*.5+.5)*[self.width,self.height]
                pad=np.ceil(np.array([self.width,self.height])*.0167)+2
                low=np.maximum(0,np.floor(xy.min(axis=0)-pad)).astype(int)
                high=np.minimum([self.width,self.height],np.ceil(xy.max(axis=0)+pad)).astype(int)
                rect=[int(low[0]),int(low[1]),int(high[0]-low[0]),int(high[1]-low[1])]
                assert rect[2]*rect[3] < self.width*self.height
            g.fn('glCopyTexSubImage2D',None,g.U,g.I,g.I,g.I,g.I,g.I,g.I,g.I)(
                0x0DE1,0,rect[0],rect[1],rect[0],rect[1],rect[2],rect[3])
            self.scene_copies += 1
            g.active(0x84C0)
        if scene:
            g.active(0x84C1); g.bind_tex(0x0DE1,self.scene); g.active(0x84C0)
        if composite:
            g.enable(0x0BE2)
            g.fn('glBlendFuncSeparate',None,g.U,g.U,g.U,g.U)(0x0302,0x0303,0,1)
        color_mask = g.fn('glColorMask',None,g.U,g.U,g.U,g.U)
        color_mask(*((0,0,0,0) if prime else (1,1,1,1)))
        g.draw(0x0004,0,len(draw_verts))
        color_mask(1,1,1,1)
        g.disable(0x0BE2)
        g.active(0x84C1); g.bind_tex(0x0DE1,0); g.active(0x84C0)
        result = np.empty((self.height,self.width,4),np.float32)
        g.read(0,0,self.width,self.height,0x1908,0x1406,result.ctypes.data)
        if depth_test:
            self.last_depth=np.empty((self.height,self.width),np.float32)
            g.read(0,0,self.width,self.height,0x1902,0x1406,self.last_depth.ctypes.data)
        for ix in used: self.disable_attr(ix)
        assert g.fn('glGetError',g.U)() == 0, 'OpenGL render error'
        assert np.isfinite(result).all(), 'nonfinite pixels'
        return result


def premultiplied(a):
    """Compare visible energy AND coverage, including partially covered details."""
    b = a.copy()
    b[..., :3] *= b[..., 3:4]
    return b


def fractional_camera_check(runner, pr, look):
    # A 4x4 stratified pixel-area reference uses the production shader with the
    # SAME footprint/LOD. Jitter integrates its residual sample-phase variation;
    # no image interpolation hides partial dots/glyphs/facets. Fixed time keeps
    # intentional Datamosh epoch changes out of the spatial measurement.
    dx = 2 / (runner.projection_x * runner.width)
    dy = 2 / (.85 * runner.height)
    results = []
    for phase, angle, zoom in ((.25, 0., 1.), (.5, .012, 1.015), (.75, -.009, .985)):
        options = dict(shift=phase*dx/zoom, shift_y=phase*dy/zoom,
                       rotation=angle, scale=zoom)
        actual = premultiplied(runner.render(pr, look, **options))
        reference = np.zeros_like(actual)
        for y in (-.375, -.125, .125, .375):
            for x in (-.375, -.125, .125, .375):
                sample = dict(options, shift=options['shift']+x*dx/(zoom*np.cos(angle)),
                              shift_y=options['shift_y']+y*dy/zoom-x*dx*np.tan(angle)/zoom)
                reference += premultiplied(runner.render(pr, look, **sample)) / 16
        # Include low/partial effect coverage, but omit silhouette/mask boundary
        # pixels where a single-sample geometry rasterizer cannot integrate area.
        geometry = runner.render(runner.probe, look, **options)[..., 3] > .99
        geometry &= runner.render(pr, 75, **options)[..., 3] > .8
        for axis in (0, 1):
            geometry &= np.roll(geometry, 2, axis) & np.roll(geometry, -2, axis)
        valid = geometry & ((actual[...,3] > .01) | (reference[...,3] > .01))
        error = np.abs(actual[valid] - reference[valid])
        p99, rms = float(np.quantile(error, .99)), float(np.sqrt(np.mean(error**2)))
        results.append(dict(phase=phase, rotation=angle, zoom=zoom, p99=p99, rms=rms,
                            samples=int(valid.sum())))
        assert valid.any() and p99 < .12 and rms < .025, (look, 'fractional camera alias', results[-1])
    return results


def datamosh_material_checks(runner, pr):
    g = runner.gl
    original = runner.diffuse
    allocated = []
    results = {}
    def make(pixels, filtering):
        tex = runner.texture(pixels, 128, 128, 0x8814, filtering)
        allocated.append(tex)
        return tex
    def render(tex, **kw):
        runner.diffuse = tex
        return runner.render(pr, 75, **kw)
    try:
        # Constant white isolates authored MAP_COLOR/vertex RGB from texture RGB.
        white = make(np.ones((128,128,4),np.float32), 'mip')
        tint = (.1,.2,.3,1.)
        a = render(white, tint=tint)
        valid = a[...,3] > 0
        expected = np.array(tint[:3]) * (.42,.72,.94)
        error = float(np.max(np.abs(a[valid,:3] - expected)))
        assert error < 1e-5, ('Datamosh authored RGB lost',error)
        results['authored_tint_max_error'] = error
        for filtering in ('nearest','linear','mip'):
            clean = runner.source_pixels.copy()
            poison = clean.copy()
            poison[poison[...,3] == 0,:3] = (19.,7.,31.)
            full_alpha = clean.copy(); full_alpha[...,3] = 1
            clean_id, poison_id, full_id = [make(p, filtering) for p in (clean,poison,full_alpha)]
            # Minification selects mip levels in the mip case; fractional motion
            # puts bilinear taps between texel centres in both filtered cases.
            opts = dict(scale=.2*768/runner.height if filtering == 'mip' else 1., shift=.0031)
            a, b = render(clean_id, **opts), render(poison_id, **opts)
            delta = float(np.max(np.abs(a-b)))
            assert np.array_equal(a[...,3],b[...,3]), 'hidden RGB changed mask coverage'
            if filtering == 'nearest':
                assert delta <= 1/255, 'Datamosh nearest transparent RGB bleed'
            else:
                assert delta > 1/255, ('filtered poison fixture missed edges',filtering)
            # Raw straight-RGB filtering IS poison-sensitive. Record this instead
            # of claiming alpha weights undo contamination inside a filtered tap.
            # An alpha-safe premultiplied reference first zeros hidden RGB, filters
            # colour*coverage and coverage together, then unpremultiplies. For a
            # constant visible colour its straight RGB is constant at every mip.
            reference = clean.copy(); reference[...,:3] = (.3,.5,.7)
            ref_id = make(reference, filtering)
            ref = render(ref_id, tint=tint, **opts)
            covered = ref[...,3] > 0
            ref_error = float(np.max(np.abs(ref[covered,:3] - np.array((.3,.5,.7))*expected)))
            assert ref_error < 1e-5, ('filtered constant-colour reference',filtering,ref_error)
            # Compare extra-tap edge energy against that analytical premultiplied
            # reference, with identical visible RGB and coverage in both inputs.
            black_edge = reference.copy()
            black_edge[black_edge[...,3] == 0,:3] = 0
            poison_edge = black_edge.copy()
            poison_edge[poison_edge[...,3] == 0,:3] = (19.,7.,31.)
            black_image = render(make(black_edge,filtering), tint=tint, **opts)
            poison_image = render(make(poison_edge,filtering), tint=tint, **opts)
            edge_errors = [float(np.max(np.abs(premultiplied(image)-premultiplied(ref))))
                           for image in (black_image,poison_image)]
            if filtering == 'nearest':
                assert max(edge_errors) < 1e-5
            else:
                assert edge_errors[1] > 1/255, 'poison/reference comparison missed filtered edge'
            # OPAQUE ignores unused base-colour alpha under every filter/LOD.
            a = render(clean_id, opaque=True, mask=False, **opts)
            b = render(full_id, opaque=True, mask=False, **opts)
            opaque_error = float(np.max(np.abs(a-b)))
            assert opaque_error < 1e-6, ('OPAQUE alpha weighted colour',filtering,opaque_error)
            # Scalar slot replay: both selected and rejected adjacent triangles.
            selected = render(ref_id, slot=0, adjacent_slots=True, **opts)
            other = render(ref_id, slot=1, adjacent_slots=True, **opts)
            whole = render(ref_id, adjacent_slots=True, **opts)
            assert np.any(selected[...,3] > 0) and np.any(other[...,3] > 0)
            slot_error = float(np.max(np.abs(selected + other - whole)))
            assert slot_error < 1e-5, ('scalar material slot replay',filtering,slot_error)
            results[filtering] = dict(raw_hidden_rgb_max_delta=delta,
                                     premultiplied_reference_max_error=ref_error,
                                     black_edge_reference_max_error=edge_errors[0],
                                     poison_edge_reference_max_error=edge_errors[1],
                                     opaque_alpha_max_error=opaque_error, slot_replay_max_error=slot_error)
    finally:
        runner.diffuse = original
        delete = g.fn('glDeleteTextures', None, g.I, C.POINTER(g.U))
        delete(len(allocated), (g.U * len(allocated))(*allocated))
    return results


def phase2_checks(runner, pr, out):
    def save_raw(a, name):
        rgb = a[...,:3] if np.all(a[...,3] > .99) else (
            a[...,:3]*np.clip(a[...,3:4],0,1)+.12*(1-np.clip(a[...,3:4],0,1)))
        Image.fromarray((np.clip(rgb[::-1],0,1)*255+.5).astype(np.uint8)).save(out/name)
    glass = runner.render(pr,64,scene=True,composite=True)
    class NoBend:
        def read(self, path):
            text = G.SourceTree('WORKTREE').read(path)
            if path == F:
                before = 'vec2 bend = n.xy * (1.0 - facing * 0.75) * 0.0167;'
                assert text.count(before) == 1
                text = text.replace(before, 'vec2 bend = vec2(0.0);')
            return text
    straight = runner.render(runner.program(NoBend()),64,scene=True,composite=True)
    bend_error = float(np.max(np.abs(glass-straight)))
    assert bend_error > .1, ('Glass did not bend background',bend_error)
    save_raw(glass,'phase2_glass_refraction.png')
    save_raw(straight,'phase2_glass_zero_bend_control.png')
    Image.fromarray((runner.background[::-1,:,:3]*255+.5).astype(np.uint8)).save(out/'phase2_glass_background.png')
    results = dict(glass_bend_max_delta=bend_error)
    # Two-clone regressions, front Glass at full opacity so stale scene pixels
    # cannot be disguised by alpha blending the rear clone through unchanged.
    front = dict(scene=True,composite=True,mask=False,opacity=1.,scale=.85,scene_rect=True)
    bare = runner.render(pr,64,**front)
    two_clone = {}
    for rear,name in ((1,'clone'),(64,'glass')):
        runner.render(pr,rear,scene=rear==64,composite=True,mask=False,
                      shift=.25,scale=.8,tint=(1.,.1,.06,1.),opacity=1.)
        composed = runner.render(pr,64,keep=True,**front)
        save_raw(composed,'phase2_glass_in_front_of_'+name+'.png')
        # The negative image control reuses bare-scene pixels, as the old
        # pre-overlay copy did. Require evidence INSIDE front Glass coverage.
        runner.render(pr,64,**front)
        runner.render(pr,rear,scene=False,composite=True,mask=False,
                      shift=.25,scale=.8,tint=(1.,.1,.06,1.),opacity=1.)
        stale = runner.render(pr,64,keep=True,stale_scene=True,**front)
        silhouette = runner.render(runner.probe,1,scale=.85,mask=False)[...,3] > .9
        difference = np.max(np.abs(composed[...,:3]-bare[...,:3]),axis=2)
        restored = np.max(np.abs(composed[...,:3]-stale[...,:3]),axis=2)
        pixels = int(np.count_nonzero(silhouette & (difference>.01) & (restored>.01)))
        assert pixels > 100, ('rear clone erased inside front Glass',name,pixels)
        save_raw(stale,'phase2_glass_in_front_of_'+name+'_stale_control.png')
        two_clone[name] = dict(rear_visible_inside_front_pixels=pixels,
                              stale_control_max_delta=float(np.max(restored[silhouette])))
    results['two_clone'] = two_clone
    # A linear UV ramp gives a directional oracle independent of the shader's
    # offset code: +screen X trails towards lower red; +screen Y towards higher
    # green (the fixture's V coordinate runs downward). Nearest sampling and
    # broad interior averaging tolerate macroblock on/off and DC/chroma taps.
    original = runner.diffuse
    y,x = np.mgrid[:128,:128]/127.
    ramp = np.stack((x,y,np.ones_like(x)*.5,np.ones_like(x)),-1).astype(np.float32)
    runner.diffuse = runner.texture(ramp,128,128,0x8814,'linear')
    try:
        horizontal = runner.render(pr,75,velocity=(12.,0.),mask=False)
        vertical = runner.render(pr,75,velocity=(0.,12.),mask=False)
        save_raw(horizontal,'phase2_datamosh_velocity_right.png')
        save_raw(vertical,'phase2_datamosh_velocity_up.png')
        h,w = runner.height,runner.width
        center = np.s_[h*3//8:h*5//8,w*7//16:w*9//16,:3]
        delta = np.mean(horizontal[center]-vertical[center],axis=(0,1))
        assert delta[0] < -.002 and delta[1] < -.002, ('smear did not follow motion',delta)
        results['velocity_right_minus_up_rgb'] = delta.tolist()
    finally:
        runner.diffuse = original
    copies = runner.scene_copies
    for look in range(78):
        if look == 64: continue
        runner.render(pr,look)
    assert runner.scene_copies == copies
    results['off_path_copy_delta'] = 0
    print('PASS phase 2: patterned refraction, motion direction, inert off-path',flush=True)
    return results


def avatar_animation_checks(runner, tree, base, out):
    """Real metre-scale body, alpha-cutout hair cards and a rigid attachment.

    These execute production GLSL with a held skin palette/attachment matrix.
    Synthetic geometry/materials are independent of the viewer's collector;
    overlay styles deliberately retain their existing MASK-only hair policy.
    """
    def ellipsoid(center, radii):
        def point(i,j):
            a,b=np.pi*i/24,np.pi+np.pi*j/48
            n=np.array((np.sin(a)*np.cos(b),np.sin(a)*np.sin(b),np.cos(a)))
            normal=n/np.array(radii);normal/=np.linalg.norm(normal)
            return [*(n*np.array(radii)+center),*normal,j/48,i/24]
        return [point(a,b) for i in range(24) for j in range(48)
                for a,b in ((i,j),(i+1,j),(i,j+1),(i,j+1),(i+1,j),(i+1,j+1))]
    body=[]
    for center,radii in (((0,0,.18),(.24,.14,.39)),((0,0,.78),(.14,.13,.20)),
                         ((-.13,0,-.56),(.09,.10,.44)),((.13,0,-.56),(.09,.10,.44)),
                         ((-.33,0,.18),(.08,.09,.41)),((.33,0,.18),(.08,.09,.41))):
        body.extend(ellipsoid(center,radii))
    body=np.array(body,np.float32) # 1.98m tall, feet at -1m
    # Two long overlapping cutout hair cards at different depths; fully clear
    # texels plus fractional alpha in a separate material fixture below.
    def card(x0,x1,y,z0,z1):
        points=[(x0,y,z0,0,1),(x1,y,z0,1,1),(x0,y,z1,0,0),(x1,y,z1,1,0)]
        return [[*points[i][:3],0,-1,0,*points[i][3:]] for i in (0,1,2,2,1,3)]
    hair=np.array(card(-.20,.10,-.16,.33,1.02)+card(-.04,.22,-.18,.26,1.01),np.float32)
    rigid=np.array(ellipsoid((0,0,0),(.5,.5,.5)),np.float32)
    palette=np.eye(4,dtype=np.float32);palette[:3,3]=(.015,.01,0)
    attachment=np.array([[.28,0,0,.34],[0,.18,0,-.12],[0,0,.38,.10],[0,0,0,1]],np.float32)
    animated=(2,10,13,15,16,17,20,21,22,23,24,26,27,65,69,70,73,74,75,76,77)
    static=(0,1,4,5,6,7,8,9,11,12,14,18,19,25,64,66,67,71)
    programs={label:(runner.program(src,('HAS_SKIN',)),runner.program(src))
              for label,src in (('before',base),('after',tree))}
    original=runner.diffuse
    y,x=np.mgrid[:128,:128]/127.
    alpha=np.where((np.sin(x*36)>-.25)&~((x>.43)&(x<.57)),.7,0.)
    hair_tex=runner.texture(np.stack((.22+.2*x,.1+.1*y,.055+.05*x,alpha),-1).astype(np.float32),128,128,0x8814)
    common=dict(raw_time=True,scale=1.05,held_pose=(.015,-.01,.08),depth_test=True)
    def frame(label,look,time,fps,fade=1.):
        skin,rigid_pr=programs[label]
        params=(1.,.8,1.,6.) if look==2 else (0.,1.,0.,8.)
        opts=dict(common,time=time,effect_fps=fps,params=params,opacity=.85*fade)
        parts=((skin,body,None,False),(skin,hair,None,True),(rigid_pr,rigid,attachment,False))
        try:
            # Prime all solid/masked geometry, then shade at LEQUAL without
            # depth writes. This catches time-dependent coverage/depth errors.
            for index,(pr,mesh,matrix,mask) in enumerate(parts):
                runner.diffuse=hair_tex if mask else original
                runner.render(pr,1,mesh=mesh,palette=palette,object_matrix=matrix,
                              mask=mask,opaque=not mask,prime=True,keep=index>0,
                              preserve_depth=index>0,**opts)
            for pr,mesh,matrix,mask in parts:
                runner.diffuse=hair_tex if mask else original
                image=runner.render(pr,look,mesh=mesh,palette=palette,object_matrix=matrix,
                                    mask=mask,opaque=not mask,keep=True,preserve_depth=True,composite=True,**opts)
            return image
        finally:
            runner.diffuse=original
    def save(a,name):
        Image.fromarray((np.clip(a[::-1,:,:3],0,1)*255+.5).astype(np.uint8)).save(out/name)
    metrics=[]
    for look in animated+static:
        for fps in (0.,8.):
            # Scan Build deliberately dwells at the fully built endpoint.
            # Sample its moving front, not that static part of the cycle.
            start=7. if look==76 else 2.
            times=tuple(start+t for t in (.01,.04,.26,.61))
            after=[frame('after',look,t,fps) for t in times]
            delta=np.max(np.abs(after[0][...,:3]-after[-1][...,:3]),axis=2)
            changed=int(np.count_nonzero(delta>1/255))
            if look in animated:
                assert changed>15, ('avatar animation frozen',look,fps,changed,float(delta.max()))
                # Same geometry and a deliberately held time must be exact.
                assert np.array_equal(after[0],frame('after',look,times[0],fps)), ('held-time negative control',look)
                faded0=frame('after',look,times[0],fps,.45)
                faded1=frame('after',look,times[-1],fps,.45)
                assert np.max(np.abs(faded0-faded1))>1/255, ('faded animation frozen',look,fps)
                if fps:
                    assert np.array_equal(after[0],after[1]), ('fps not held within step',look)
                elif look==69:
                    smooth_delta=np.max(np.abs(after[0][...,:3]-after[1][...,:3]),axis=2)
                    assert np.count_nonzero(smooth_delta>1/255)>15, 'Magma smooth flow did not advance between 30ms frames'
                before=[frame('before',look,t,fps) for t in times]
                for label,images in (('before',before),('after',after)):
                    for index,img in enumerate(images):
                        save(img,'avatar_%s_%02d_fps%02d_%02d.png'%(label,look,fps,index))
            else:
                assert np.array_equal(after[0],after[-1]), ('static negative control moves',look,fps)
            metrics.append(dict(look=look,fps=fps,animated=look in animated,
                                changed_pixels=changed,max_delta=float(delta.max())))
            print('PASS avatar time/fps/held/fade:',look,fps,changed,flush=True)
    # Magma must move actual crack positions, not only modulate fixed cracks.
    class FlowOnly:
        def read(self,path):
            source=tree.read(path)
            if path==F:
                pulse='float pulse = 0.65 + 0.35 * sin(t * 2.4 + P.z * 3.0 + ghostAux.w);'
                assert source.count(pulse)==1
                source=source.replace(pulse, 'float pulse = 0.8;')
            return source
    class NoFlow(FlowOnly):
        def read(self,path):
            source=super().read(path)
            if path==F:
                source=source.replace('float t = ghostTime;', 'float t = 0.0;')
            return source
    for label,src in (('flow_only',FlowOnly()),('no_flow',NoFlow())):
        programs[label]=(runner.program(src,('HAS_SKIN',)),runner.program(src))
    flow_delta=float(np.max(np.abs(frame('flow_only',69,2.01,0)-frame('flow_only',69,2.61,0))))
    assert flow_delta>.05, ('Magma cracks do not flow with heat pulse held constant',flow_delta)
    assert np.array_equal(frame('no_flow',69,2.01,0),frame('no_flow',69,2.61,0)), 'no-flow negative control moves'
    class FrozenClock:
        def read(self,path):
            source=tree.read(path)
            if path==F:
                assert source.count('uniform float ghostTime;')==1
                source=source.replace('uniform float ghostTime;', 'const float ghostTime = 2.01;')
            return source
    programs['frozen_clock']=(runner.program(FrozenClock(),('HAS_SKIN',)),runner.program(FrozenClock()))
    for look in animated:
        a,b=frame('frozen_clock',look,2.01,0),frame('frozen_clock',look,2.61,0)
        assert np.array_equal(a,b), ('frozen-clock negative control moves',look)
    # Prove alpha hair fixture contains visible strands and holes in isolation.
    try:
        runner.diffuse=hair_tex
        h=runner.render(programs['after'][0],1,mesh=hair,palette=palette,mask=False,**common)
        assert np.any((h[...,3]>.1)&(h[...,3]<.8)) and np.any(h[...,3]==0), 'hair alpha control insensitive'
        save(h,'avatar_alpha_hair_control.png')
    finally:
        runner.diffuse=original
    return dict(height_metres=1.98,body_vertices=len(body),hair_vertices=len(hair),
                rigid_vertices=len(rigid),cases=metrics,magma_flow_only_max_delta=flow_delta,
                note='Held skin palette + rigid matrix; cutoff alpha hair, existing overlay SOLID policy. No viewer collector/in-world claim.')


def four_k_check(out):
    r = Runner(3840,2160)
    g = r.gl
    pr = r.program(G.SourceTree('WORKTREE'))
    glass = r.render(pr,64,scene=True,composite=True)
    assert glass.shape == (2160,3840,4)
    assert np.max(np.abs(glass-r.background)) > .1
    Image.fromarray((np.clip(glass[::-1,:,:3],0,1)*255+.5).astype(np.uint8)).save(out/'phase2_glass_4k.png')
    # TIME_ELAPSED excludes Python, upload/allocation and readback. Warm the copy
    # first, then measure 32 copies (a bandwidth estimate, not frame latency).
    g.active(0x84C1); g.bind_tex(0x0DE1,r.scene)
    copy = g.fn('glCopyTexSubImage2D',None,g.U,g.I,g.I,g.I,g.I,g.I,g.I,g.I)
    for _ in range(4): copy(0x0DE1,0,0,0,0,0,3840,2160)
    query = g.U()
    g.fn('glGenQueries',None,g.I,C.POINTER(g.U))(1,C.byref(query))
    g.fn('glBeginQuery',None,g.U,g.U)(0x88BF,query)
    for _ in range(32): copy(0x0DE1,0,0,0,0,0,3840,2160)
    g.fn('glEndQuery',None,g.U)(0x88BF)
    elapsed = C.c_uint64()
    g.fn('glGetQueryObjectui64v',None,g.U,g.U,C.POINTER(C.c_uint64))(query,0x8866,C.byref(elapsed))
    copy_ms = elapsed.value/1e6/32
    rect_costs = []
    for w,h in ((384,216),(960,540),(1920,1080),(3840,2160)):
        for count in (1,4,16):
            g.fn('glBeginQuery',None,g.U,g.U)(0x88BF,query)
            for _ in range(32):
                for clone in range(count): copy(0x0DE1,0,0,0,0,0,w,h)
            g.fn('glEndQuery',None,g.U)(0x88BF)
            g.fn('glGetQueryObjectui64v',None,g.U,g.U,C.POINTER(C.c_uint64))(query,0x8866,C.byref(elapsed))
            rect_costs.append(dict(rect=[w,h],clones=count,gpu_ms=elapsed.value/1e6/32,
                                   copied_bytes=w*h*4*count))
    g.fn('glDeleteQueries',None,g.I,C.POINTER(g.U))(1,C.byref(query))
    g.active(0x84C0)
    assert g.fn('glGetError',g.U)() == 0
    result = dict(size=[3840,2160], renderer=g.renderer, copy_gpu_ms=copy_ms,
                  per_glass_rect_gpu=rect_costs, vertices=len(r.verts),
                  note='Synthetic single front hemisphere; excludes viewer scene, CPU submission, resize and readback')
    print('PASS 4K copy/render; GPU timings:',json.dumps(result),flush=True)
    return result


def main():
    from clone_looks_runtime_check import check as runtime_check
    runtime_check()
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base', default='4afbf00ba09')
    ap.add_argument('--width', type=int, default=768)
    ap.add_argument('--height', type=int, default=768)
    ap.add_argument('--out', type=Path, default=ROOT/'.tmp/ashfix_20261102/renders')
    args = ap.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    runner = Runner(args.width,args.height)
    tree, base = G.SourceTree('WORKTREE'), G.SourceTree(args.base)
    stats = dict(renderer=runner.gl.renderer, size=[args.width,args.height], regressions=[], looks={})
    print('GL:',runner.gl.renderer,flush=True)
    world = ('GHOST_WORLD_PASS','GHOST_SHARED_DISSOLVE')
    permutations = [('studio',(),V),('studio_skin',('HAS_SKIN',),V),
                    ('world_no_shared',('GHOST_WORLD_PASS',),V),
                    ('world',world,V),('world_skin',world+('HAS_SKIN',),V),
                    ('indexed',world+('GHOST_INDEXED_WORLD',),V),
                    ('indexed_skin',world+('GHOST_INDEXED_WORLD','HAS_SKIN'),V),
                    ('system',world+('GHOST_SYSTEM_AVATAR',),SH+'avatar/avatarActorGhostV.glsl'),
                    ('system_cloth',world+('GHOST_SYSTEM_AVATAR','AVATAR_CLOTH'),SH+'avatar/avatarActorGhostV.glsl'),
                    ('eyeball',world+('GHOST_SYSTEM_AVATAR',),SH+'avatar/eyeballActorGhostV.glsl')]
    for label,defs,vertex in permutations:
        pr = runner.program(tree,defs,vertex)
        if label == 'world_no_shared':
            # Viewer always pairs WORLD with SHARED. The base shader references
            # rest position in Tron without declaring it in this unused variant.
            print('PASS compile/link:',label,'(not a base/viewer permutation)',flush=True)
            continue
        old = runner.program(base,defs,vertex)
        maximum = 37 if 'GHOST_WORLD_PASS' in defs else 28
        worst = 0.
        for look in range(maximum):
            a,b = runner.render(old,look),runner.render(pr,look)
            assert np.any(a[...,3] > .001), (label,look,'empty regression fixture')
            delta = float(np.max(np.abs(a-b)))
            worst = max(worst,delta)
            assert delta == 0., (label,look,delta)
        stats['regressions'].append(dict(permutation=label,looks=maximum,max_error=worst))
        print('PASS compile/link + regression:',label,flush=True)
    # Exercise blend-disabled opaque/mask sweeps, including coverage/depth holes.
    fade_stats = []
    for defs in ((), ('HAS_SKIN',)):
        fade_pr = runner.program(tree, defs)
        for mask in (False, True):
            full = runner.render(fade_pr, 1, mask=mask, opaque=True, depth_test=True)
            full_depth=runner.last_depth.copy()
            zero = runner.render(fade_pr, 1, mask=mask, opaque=True, fade=0.,depth_test=True)
            assert np.all(runner.last_depth==1.), "fade zero writes depth"
            half = runner.render(fade_pr, 1, mask=mask, opaque=True, fade=.5,depth_test=True)
            half_depth=runner.last_depth.copy()
            assert not np.any(zero), ('fade zero drew geometry', defs, mask)
            assert np.array_equal(full, runner.render(fade_pr,1,mask=mask,opaque=True,fade=1.)), 'fade one parity'
            occupied = full[...,3] > 0
            surviving = half[...,3] > 0
            assert np.array_equal(half_depth[surviving],full_depth[surviving]), 'surviving depth changed'
            assert np.all(half_depth[~surviving]==1.), 'discarded coverage writes depth'
            ratio = np.count_nonzero(surviving)/np.count_nonzero(occupied)
            assert .49 < ratio < .51, ('half coverage', ratio)
            assert np.array_equal(full[surviving], half[surviving]), 'fade changed surviving beauty'
            assert np.array_equal(half,runner.render(fade_pr,1,mask=mask,opaque=True,fade=.5,time=900.)), 'coverage crawls'
            fade_stats.append(dict(skin=bool(defs),mask=mask,half_coverage=ratio,endpoint_error=0.))
    # Link the actual native material/coverage modules. Analytic directional lit
    # material is the boundary fixture, not a claim of an in-world PBR test.
    class NativeTree:
        def read(self, path):
            if path != F: return tree.read(path)
            module = tree.read(SH+'alchemy/actorFxF.glsl').replace('vary_actor_fx_position','vary_object_position')
            coverage = tree.read(SH+'alchemy/actorFxDissolveF.glsl')
            coverage = '\n'.join(line for line in coverage.splitlines() if not (line.startswith('uniform ') and line in module))
            return module + coverage + r"""
                in vec3 vary_normal; in vec2 vary_texcoord0;
                uniform sampler2D diffuseMap; uniform vec4 ghostAux;
                out vec4 frag_color;
                void main(){
                    bool styled = actorFxActive();
                    vec4 material=texture(diffuseMap,vary_texcoord0);
                    if (material.a < ghostAux.x) discard;
                    float light=.08+.9*max(dot(normalize(vary_normal),normalize(vec3(.7,.2,1.))),0.);
                    frag_color=vec4(material.rgb*light,1.);
                }
            """
    native_pr = runner.program(NativeTree())
    for mask in (False,True):
        lit=runner.render(native_pr,1,mask=mask,native_fade=True)
        zero=runner.render(native_pr,1,mask=mask,fade=0,native_fade=True)
        half=runner.render(native_pr,1,mask=mask,fade=.5,native_fade=True)
        assert not np.any(zero), 'deferred zero drew'
        visible=half[...,3]>0
        assert np.array_equal(lit[visible],half[visible]), 'deferred lighting changed'
        assert np.array_equal(lit,runner.render(native_pr,1,mask=mask)), 'deferred endpoint changed'
        assert .49 < np.count_nonzero(visible)/np.count_nonzero(lit[...,3]) < .51
    # Actual production silhouette-cover target/vertex shader versus beauty pixels.
    class CoverTree:
        def read(self,path):
            if path == V: return tree.read(SH+'deferred/actorMaskV.glsl')
            if path == F:
                return '#define MASK_CHANNELS 4\n'+tree.read(SH+'deferred/silCoverF.glsl')+'\nvoid mirrorClip(vec3 p){}\n'
            return tree.read(path)
    sil_cases=[]
    for defs in ((),('HAS_SKIN',)):
        beauty=runner.program(NativeTree(),defs)
        cover=runner.program(CoverTree(),defs)
        for pose in ((-.6,.2,.7),(.5,-.3,-.9)):
            for fade in (0.,.5,1.):
                for mask in (False,True):
                    b=runner.render(beauty,1,mask=mask,fade=fade,native_fade=True,held_pose=pose,depth_test=True)
                    bd=runner.last_depth.copy()
                    c=runner.render(cover,1,mask=mask,fade=fade,held_pose=pose,depth_test=True)
                    assert np.array_equal(b[...,3]>0,c[...,3]>0), ('silhouette/beauty coverage',defs,pose,fade,mask)
                    # Static native replay uses precombined MVP; Studio beauty uses
                    # projection * modelview. Coverage must be exact; float depth
                    # may differ slightly after independent triangle interpolation.
                    depth_error=float(np.max(np.abs(bd-runner.last_depth)))
                    assert depth_error <= 2e-6, ('silhouette/beauty depth',defs,pose,fade,mask,depth_error)
                    sil_cases.append(dict(skin=bool(defs),pose=pose,fade=fade,mask=mask,depth_error=depth_error))
        held=runner.render(cover,1,held_pose=(-.6,.2,.7))
        live=runner.render(cover,1,held_pose=(.5,-.3,-.9))
        assert not np.array_equal(held[...,3],live[...,3]), 'moving-pose control is insensitive'
    stats['silhouette_coverage']=sil_cases
    print('PASS: production silhouette coverage matches beauty exactly; depth error <=2e-6; moving held palettes/rigid transforms, fades 0/.5/1',flush=True)
    stats['fade_coverage'] = fade_stats
    print('PASS: opaque/masked fade 0/.5/1; stable coverage; native lit endpoint parity 0.0', flush=True)
    pr = runner.program(tree)
    for defs in ((), ('HAS_SKIN',)):
        current, previous = runner.program(tree,defs), runner.program(base,defs)
        for look in list(range(28)) + list(NAMES):
            if look == 69: continue
            for time in (2., 2.61):
                for fade in (1., .45):
                    a = runner.render(previous,look,time=time,fade=fade)
                    b = runner.render(current,look,time=time,fade=fade)
                    assert np.array_equal(a,b), ('base parity',defs,look,time,fade,float(np.max(np.abs(a-b))))
    stats['requested_base_unchanged_max_error'] = 0.0
    print('PASS requested-base parity: every surviving look except Magma, static + rigged, two times/fades: 0.0',flush=True)
    phase1 = runner.program(G.SourceTree('24b2723dcd9'))
    unchanged = [look for look in range(78) if look not in (64,68,69,72,75)]
    parity = max(float(np.max(np.abs(runner.render(pr,look)-runner.render(phase1,look))))
                 for look in unchanged)
    assert parity == 0., ('phase 1 unchanged looks',parity)
    stats['phase1_unchanged_max_error'] = parity
    skin = runner.program(tree,('HAS_SKIN',))
    skin_phase1 = runner.program(G.SourceTree('24b2723dcd9'),('HAS_SKIN',))
    skin_parity = max(float(np.max(np.abs(runner.render(skin,look)-runner.render(skin_phase1,look))))
                      for look in unchanged)
    assert skin_parity == 0., ('phase 1 unchanged rigged looks',skin_parity)
    stats['phase1_unchanged_rigged_max_error'] = skin_parity
    print('PASS phase-1 parity: all 73 unaffected Studio ids, static + rigged max error',parity,skin_parity,flush=True)
    runner.probe = runner.program(tree, probe=True)
    def save(a,name):
        rgb = a[...,:3]*np.clip(a[...,3:4],0,1)+.12*(1-np.clip(a[...,3:4],0,1))
        Image.fromarray((np.clip(rgb[::-1],0,1)*255+.5).astype(np.uint8)).save(args.out/name)
    for look,name in NAMES.items():
        a = runner.render(pr,look)
        b = runner.render(pr,look,time=2+1/60)
        shift = 2/(runner.projection_x*args.width) # exactly one screen pixel
        c = runner.render(pr,look,shift=shift)
        assert np.count_nonzero(a[...,3] > .01) > 100, (look,'empty')
        assert np.max(a[...,:3]) > .01, (look,'black')
        # Every look must retain the texture-alpha hole, independent of style.
        mid = a[args.height//2,args.width//2]
        assert mid[3] == 0, (look,'alpha mask filled',mid)
        temporal = float(np.quantile(np.abs(a-b),.99))
        if look in (66,67,71):
            assert temporal <= 1/255, (look,'static detail crawls',temporal)
        moved = np.roll(c,-1,axis=1)
        interior = (a[...,3]>.8)&(moved[...,3]>.8)
        camera = float(np.quantile(np.abs(a[interior]-moved[interior]),.99)) if interior.any() else 0.
        if look in (65,66,67,69,73,74,76):
            assert interior.any(), (look,'no camera comparison samples')
            assert camera < .03, (look,'body field camera swim',camera)
        fractional = fractional_camera_check(runner, pr, look) if look in (65,66,67,69,73,74,76) else []
        # Exercise all independent distortion modes with each look.
        for distort in range(1,9):
            d = runner.render(pr,look,distort=distort)
            assert np.any(d[...,3]>.001), (look,distort,'empty distortion')
        # Far lattice still renders without changing cell identity in source.
        far = runner.render(pr,look,scale=.2)
        assert np.any(far[...,3]>.001), (look,'empty LOD')
        if look in (67,73):
            far_moved = np.roll(runner.render(pr,look,scale=.2,shift=shift/.2),-1,axis=1)
            valid = (far[...,3]>.1)&(far_moved[...,3]>.1)
            assert np.quantile(np.abs(far[valid]-far_moved[valid]),.99)<.03, (look,'far camera swim')
        stem = '%02d_%s' % (look,name.lower().replace(' ','_'))
        for data,suffix in ((a,''),(b,'_next'),(c,'_camera'),(far,'_far')):
            save(data,stem+suffix+'.png')
        stats['looks'][str(look)] = dict(name=name,fractional_camera=fractional,temporal_p99=temporal,camera_p99=camera,alpha_pixels=int(np.count_nonzero(a[...,3])),png=stem+'.png')
        print('PASS render/mask/time/camera/distort/LOD:',look,name,flush=True)
    for look in (0,1,9,10,27):
        save(runner.render(pr,look),'regression_%02d.png'%look)
    # Rest-coordinate transport: exact same points after camera translation.
    probe=runner.program(tree,probe=True)
    a=runner.render(probe,66)
    b=np.roll(runner.render(probe,66,shift=2/(runner.projection_x*args.width)),-1,axis=1)
    valid=(a[...,3]>0)&(b[...,3]>0)
    assert np.max(np.abs(a[valid]-b[valid]))<1e-4, 'rest varying swims'
    stats['avatar_animation'] = avatar_animation_checks(runner, tree, base, args.out)
    stats['datamosh_materials'] = datamosh_material_checks(runner, pr)
    stats['phase2'] = phase2_checks(runner, pr, args.out)
    # Pin fixed-lattice policy; a camera footprint may fade, never scale cells.
    source=tree.read(F)
    for look in (67,73):
        branch=source.split('else if (ghostLook == %d)'%look)[1].split('else if')[0]
        assert re.search(r'float cs\s*=\s*0\.',branch), 'camera-dependent lattice'
    # Regenerate contact sheets from this run, never from stale thumbnails.
    sheet = Image.new('RGB', (1600, 4*292), '#1f1f1f')
    draw = ImageDraw.Draw(sheet)
    for index, (look, name) in enumerate(NAMES.items()):
        stem = '%02d_%s' % (look,name.lower().replace(' ','_'))
        with Image.open(args.out/(stem+'.png')) as im:
            im.thumbnail((384,256), Image.Resampling.LANCZOS)
            x, y = (index%4)*400, (index//4)*292
            sheet.paste(im, (x+(400-im.width)//2, y+28+(256-im.height)//2))
            draw.text((x+10,y+8), '%d %s' % (look,name), fill='white')
    sheet.save(args.out/'contact_sheet.png')
    stats['4k'] = four_k_check(args.out)
    (args.out/'metrics.json').write_text(json.dumps(stats,indent=2)+'\n',encoding='utf-8')
    print('PASS: all clone look GL checks; PNGs and metrics:',args.out,flush=True)


if __name__ == '__main__':
    main()
