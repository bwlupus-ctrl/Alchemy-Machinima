"""Hidden WGL compilation/render regression for production Studio clone shaders.

No viewer build. Uses relight_3d_gl's real driver/context, the complete production
vertex/fragment sources and production skin modules. External atmosphere and
indexed sampler plumbing are identity fixtures (not a viewer pipeline test).
The mesh is a front ellipsoid in a conventional z-up bind frame; it cannot
validate every imported attachment's height/origin. Writes float-test metrics
and composited PNG previews; failures raise, including unavailable OpenGL.
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
NAMES = ['Glass', 'Mercury', 'Marble', 'Crystal', 'Ash Drift', 'Magma',
         'Void', 'Sumi-e', 'Arc', 'Lidar', 'Code Rain', 'Datamosh',
         'Scan Build', 'Contour']


class Runner:
    def __init__(self, width, height):
        self.gl = g = G.GL()
        self.definitions = {}
        self.width, self.height = width, height
        self.projection_x = .7 * height / width
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0):
            g.disable(cap)
        self.clear = g.fn('glClearBufferfv', None, g.U, g.I, C.POINTER(g.F))
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
        assert g.fbo_status(0x8D40) == 0x8CD5
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
               shift_y=0., rotation=0., slot=-1, adjacent_slots=False):
        g = self.gl
        defs, vertex = self.definitions[pr]
        if look == 68:
            time += 7.0  # Sample the burn front mid-cycle, not its empty endpoint.
        if look == 72:
            time -= .105  # Capture an arc near its attack as well as its decay.
        if look == 76:
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
            floats(name, .42, .72, .94, .85)
        floats('ghostTime', time)
        floats('ghostParams', 0., 1., 0., 8.)
        floats('ghostAux', .5 if mask else 0., 1., 0., phase)
        floats('ghostFx', 0., 0., 0., 1.)
        floats('ghostAlpha', 0. if opaque else 1., 1.)
        floats('ghostDistortParams', .4 if distort else 0., .5, .5, 0.)
        floats('ghostScreenSize', self.width, self.height)
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
        # Camera roll and zoom also exercise transformed pattern footprints.
        co, si = np.cos(rotation), np.sin(rotation)
        roll = np.array([[co,-si,0,0],[si,co,0,shift_y],[0,0,1,0],[0,0,0,1]], np.float32)
        view = roll @ view
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
            a = np.array([[1,0,0,0],[0,1,0,0],[0,0,1,0]],np.float32)
            self.mat34(loc('matrixPalette[0]'), 1, 0, a.ctypes.data_as(C.POINTER(g.F)))
        g.bind_buf(0x8892, self.vbo)
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
        self.clear(0x1800,0,(g.F*4)(0,0,0,0))
        g.draw(0x0004,0,len(self.verts))
        result = np.empty((self.height,self.width,4),np.float32)
        g.read(0,0,self.width,self.height,0x1908,0x1406,result.ctypes.data)
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
    # intentional Arc/Datamosh epoch changes out of the spatial measurement.
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


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base', default='c5dd757d341')
    ap.add_argument('--width', type=int, default=768)
    ap.add_argument('--height', type=int, default=768)
    ap.add_argument('--out', type=Path, default=ROOT/'.tmp/clone_looks_20261101/renders')
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
            assert delta <= 1/255, (label,look,delta)
        stats['regressions'].append(dict(permutation=label,looks=maximum,max_error=worst))
        print('PASS compile/link + regression:',label,flush=True)
    pr = runner.program(tree)
    runner.probe = runner.program(tree, probe=True)
    def save(a,name):
        rgb = a[...,:3]*np.clip(a[...,3:4],0,1)+.12*(1-np.clip(a[...,3:4],0,1))
        Image.fromarray((np.clip(rgb[::-1],0,1)*255+.5).astype(np.uint8)).save(args.out/name)
    for look,name in enumerate(NAMES,64):
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
        if look in (65,66,67,69,72,73,74,76):
            assert interior.any(), (look,'no camera comparison samples')
            assert camera < .03, (look,'body field camera swim',camera)
        fractional = fractional_camera_check(runner, pr, look) if look in (65,66,67,69,72,73,74,76) else []
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
    stats['datamosh_materials'] = datamosh_material_checks(runner, pr)
    # Pin fixed-lattice policy; a camera footprint may fade, never scale cells.
    source=tree.read(F)
    for look in (67,73):
        branch=source.split('else if (ghostLook == %d)'%look)[1].split('else if')[0]
        assert re.search(r'float cs\s*=\s*0\.',branch), 'camera-dependent lattice'
    # Regenerate contact sheets from this run, never from stale thumbnails.
    sheet = Image.new('RGB', (1600, 4*292), '#1f1f1f')
    draw = ImageDraw.Draw(sheet)
    for index, (look, name) in enumerate(zip(range(64,78), NAMES)):
        stem = '%02d_%s' % (look,name.lower().replace(' ','_'))
        with Image.open(args.out/(stem+'.png')) as im:
            im.thumbnail((384,256), Image.Resampling.LANCZOS)
            x, y = (index%4)*400, (index//4)*292
            sheet.paste(im, (x+(400-im.width)//2, y+28+(256-im.height)//2))
            draw.text((x+10,y+8), '%d %s' % (look,name), fill='white')
    sheet.save(args.out/'contact_sheet.png')
    (args.out/'metrics.json').write_text(json.dumps(stats,indent=2)+'\n',encoding='utf-8')
    print('PASS: all clone look GL checks; PNGs and metrics:',args.out,flush=True)


if __name__ == '__main__':
    main()
