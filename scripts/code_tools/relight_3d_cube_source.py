"""CubeSource: the P1-lite world cube-shadow path for the 3D reference harness (relight_3d_reference.py --source).

    py -3 -I scripts/code_tools/relight_3d_reference.py --shader-ref WORKTREE \
        --source scripts/code_tools/relight_3d_cube_source.py:CubeSource

What it stands in for, and what it does NOT:
  * The RASTER of the six widened faces is a CPU ray cast (this module): for every light that is world-resident, six
    544 x 544 faces from the shared face table (the Python mirror below; pinned against alworldshadowmath.h and the GLSL
    table by relight_shader_check.py), front faces only, the near cube clipped, the emitting prim (Light.ignore) left
    out, the result a D32F window-depth atlas laid out exactly like LLPipeline::generateWorldShadowCubes (tile column
    f % 3, row 2k + f / 3, 1632 wide). The viewer renders the same faces with the production shadow pass; the harness
    cannot, so the raster is a stated stand-in (analytic scenes: exact depth at texel centres).
  * Everything downstream is PRODUCTION: the WORKTREE areaShadowF.glsl gather (contact term for the world channel) and
    areaShadowFilterF.glsl (blur + full-resolution upsample + the world resolve), driven with exactly the uniforms
    renderAreaShadows uploads, against the same analytic G-buffer and the same 256-ray ground truth as the screen path.

Configs (see configs()): cube (Default plan), cube+thin, cube2 (two resident lights), and the diagnostics cube_worldonly
(no contact term) and cube_fixedrot (rotation candidate (c): no per-texel rotation).
Residency here is the harness stand-in for Pinned: the highest-importance eligible lights (point or projector) of the POSE, by the
production importance formula with unit luminance. Camera-only groups of scenes A and B have a single light, so the pick
cannot change inside them.
"""
import hashlib
import math
import os
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import relight_3d_scene as S  # noqa: E402
import relight_3d_gl as G  # noqa: E402
import relight_3d_plan as PL  # noqa: E402
_main = sys.modules.get('__main__')
if getattr(_main, 'VisibilitySource', None) is not None:  # loaded by relight_3d_reference.py run as a script
    VisibilitySource = _main.VisibilitySource
else:
    import relight_3d_reference as _R  # noqa: E402
    VisibilitySource = _R.VisibilitySource

np.seterr(over='ignore', invalid='ignore', divide='ignore')

CUBE_VERSION = 'cube-raster-2'
USABLE, GUTTER = 512, 16
TILE = USABLE + 2 * GUTTER          # 544
TAN_HALF = TILE / USABLE            # 1.0625: tan of half the widened FOV
ATLAS_W = 3 * TILE                  # 1632
ATLAS_H_PER_LIGHT = 2 * TILE        # 1088
LIGHT_BIAS = 0.25                   # RenderAreaShadowLightBias default: governs the SCREEN trace (the gather's housing skip) only
NEAR = 0.05                         # the cube face near plane: a fixed 5 cm (alworldshadowmath.h kNearPlane), decoupled from the Light bias
USER_CONTACT = 0.15                 # RenderAreaShadowMappedContact default

# Probe cube order +X -X +Y -Y +Z -Z (LLCubeMapArray::sTargets) and the probe up vectors (LLViewerWindow::cubeSnapshot).
FACE_AXIS = [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)]
FACE_UP_IN = [(0, -1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1), (0, -1, 0), (0, -1, 0)]


def face_basis(face):
    """(axis, right, up) exactly as glm::lookAt builds the view rows: right = norm(axis x up_in), up = right x axis."""
    a = np.array(FACE_AXIS[face], np.float64)
    r = np.cross(a, np.array(FACE_UP_IN[face], np.float64))
    r /= np.linalg.norm(r)
    return a, r, np.cross(r, a)


def near_plane(near=NEAR):
    return near


def texel_world(z):
    return 2.0 * z / USABLE


def blind_band_max(far, tan_cap=2.0):
    """alworldshadowmath.h blindBandMax (kept literal here; relight_shader_check.py pins both)."""
    return min(1.0 * texel_world(far), 0.05) + 0.003 + 1.0 * texel_world(far) * tan_cap


def contact_distance(far, user=USER_CONTACT):
    return float(min(max(max(user, blind_band_max(far)), 0.02), 0.5))


def depth_from_linear(z, n, f):
    return 0.5 * ((f + n) / (f - n) - 2.0 * f * n / ((f - n) * z)) + 0.5


def _inside(p, o):
    """The light origin lies inside prim p (every hit from there is a back face: culled, so the prim casts nothing)."""
    o = np.asarray(o, np.float64)
    if p.kind == 'obb':
        loc = (o - np.array(p.c)) @ p.rot()
        return bool(np.all(np.abs(loc) < np.array(p.h)))
    if p.kind == 'sphere':
        return bool(np.linalg.norm(o - np.array(p.c)) < p.r)
    a, b = np.array(p.a, np.float64), np.array(p.b, np.float64)
    ab = b - a
    t = np.clip(((o - a) @ ab) / (ab @ ab), 0, 1)
    return bool(np.linalg.norm(o - (a + t * ab)) < p.r)


def raster_face(scene, light, face, near, far, skip):
    """Window depth (TILE, TILE) float32 of one face: row j = ndc y ascending, like the GL render target."""
    a, r, u = face_basis(face)
    C = np.array(light.pos, np.float64)
    ij = (np.arange(TILE) + 0.5) / TILE * 2.0 - 1.0
    gx, gy = np.meshgrid(ij * TAN_HALF, ij * TAN_HALF)           # gx varies along columns, gy along rows
    d = a[None, None, :] + gx[..., None] * r[None, None, :] + gy[..., None] * u[None, None, :]
    inv_len = 1.0 / np.linalg.norm(d, axis=2)
    d = (d * inv_len[..., None]).reshape(-1, 3).astype(np.float32)
    o = np.broadcast_to(C.astype(np.float32), d.shape).copy()
    zscale = (inv_len.reshape(-1)).astype(np.float32)             # axial depth = distance * (d . axis) = distance / |d|
    best = np.full(len(d), np.inf, np.float32)
    corners = None
    for pi, p in enumerate(scene.prims):
        if pi in skip:
            continue
        lo, hi = p.aabb()
        cs = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]) - C
        z = cs @ a
        if np.all(z > 1e-6):  # fully in front: cull by the face window
            sx, sy = (cs @ r) / z, (cs @ u) / z
            if sx.min() > TAN_HALF or sx.max() < -TAN_HALF or sy.min() > TAN_HALF or sy.max() < -TAN_HALF:
                continue
        elif np.all(z <= 0):
            continue
        t = S.isect(p, o, d).astype(np.float32)
        zz = t * zscale
        ok = np.isfinite(t) & (t < 1e29) & (zz >= near) & (zz <= far)
        best = np.minimum(best, np.where(ok, zz, np.inf).astype(np.float32))
    dep = np.where(np.isfinite(best), depth_from_linear(np.where(np.isfinite(best), best, 1.0).astype(np.float64), near, far), 1.0)
    return dep.reshape(TILE, TILE).astype(np.float32)


def light_tiles(scene, light, near, far, cache_dir, noignore=False):
    """(6, TILE, TILE) D32F window depth for one light; memory + disk cached on scene content."""
    key = hashlib.sha1(repr((scene.signature(), light.spec(), near, far, noignore, CUBE_VERSION)).encode()).hexdigest()[:16]
    path = Path(cache_dir) / ('cube_%s_%s.npy' % (scene.name, key))
    if path.exists():
        return np.load(path)
    # The emitting prim never casts in its own cube (RenderSpotLight in production). A prim that ENCLOSES the light casts nothing either, whatever
    # the hook does: every triangle of a closed bulb is back-facing from inside and the shadow pass culls back faces (`noignore` = diagnostic: no
    # explicit exclusion, only that culling).
    skip = {scene.prim_index(light.ignore)} if (light.ignore and not noignore) else set()
    skip.discard(-1)
    skip |= {i for i, p in enumerate(scene.prims) if _inside(p, light.pos)}
    tiles = np.stack([raster_face(scene, light, f, near, far, skip) for f in range(6)])
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + '.%d.tmp.npy' % os.getpid())
    np.save(tmp, tiles)
    os.replace(tmp, path)
    return tiles


def importance(light, eye):
    """ALAreaShadow::importance with unit luminance, no native map: range / max(distance, range / 2) * 1.25."""
    d = float(np.linalg.norm(np.array(light.pos) - eye))
    return light.range / max(d, light.range * 0.5) * 1.25


class CubeSource(VisibilitySource):
    name = 'cube'
    # name -> (plan name, resident lights, contact term on, fixed rotation)
    # name -> (plan, resident lights, contact term, fixed rotation [, options]). The setting default is N = 2 (user decision 10/23).
    CONFIGS = [('cube', 'Default', 2, True, False),
               ('cube+thin', 'Default+Thin', 2, True, False),
               ('cube1', 'Default', 1, True, False),
               ('cube_worldonly', 'Default', 2, False, False),
               ('cube_fixedrot', 'Default', 2, True, True),
               # scene W/WB diagnostics: the superseded near plane rule max(5 cm, Light bias) = 25 cm, a 10 cm plane, and no explicit emitter exclusion
               ('cube_near025', 'Default', 2, True, False, {'near': 0.25}),
               ('cube_near010', 'Default', 2, True, False, {'near': 0.10}),
               ('cube_noignore', 'Default', 2, True, False, {'noignore': True})]

    def prepare(self, ctx):
        args = ctx['args']
        self.tree = G.SourceTree(args.shader_ref)
        if args.shader_ref != 'WORKTREE':
            print('[cube] WARNING: the cube path needs the worktree shaders (--shader-ref WORKTREE); got %s' % args.shader_ref, flush=True)
        self.plans, self.plan_source = PL.compute_plans(self.tree, ctx['out'])
        self.runner = G.AreaRunner(self.tree)
        self.filter_fixed = None
        # Harness experiments only (constant sweeps): CUBE_FILTER_SUBS="old=>new|old=>new" re-links the filter with text substitutions.
        subs = [tuple(x.split('=>')) for x in os.environ.get('CUBE_FILTER_SUBS', '').split('|') if '=>' in x]
        if subs:
            self.runner.filter = self.runner._link('class3/deferred/areaShadowFilterF.glsl', subs=subs)
        self.cache_dir = Path(ctx['out']) / 'cube_cache'
        self.atlas_cache = {}
        ctx['renderer'] = self.runner.gl.renderer
        ctx['shader_commit'] = self.tree.commit()
        ctx['plan_source'] = self.plan_source
        only = os.environ.get('CUBE_CONFIGS')
        self.enabled = [c for c in self.CONFIGS if not only or c[0] in only.split(',')]

    def configs(self, ctx):
        return [(c[0], {'plan': c[1], 'n': c[2], 'contact': c[3], 'fixedrot': c[4],
                        'near': (c[5] if len(c) > 5 else {}).get('near', NEAR),
                        'bias': (c[5] if len(c) > 5 else {}).get('bias', LIGHT_BIAS),
                        'noignore': (c[5] if len(c) > 5 else {}).get('noignore', False)}) for c in self.enabled]

    def plan_text(self, name):
        for c in self.CONFIGS:
            if c[0] == name:
                p = self.plans[c[1]][2]
                return 'cube N=%d + %d rays x %d steps' % (c[2], p['samples'], p['steps'])
        return ''

    # ---- residency stand-in
    def resident(self, frame, n):
        eye = frame.cam.eye
        # Point lights AND projectors are eligible (user decision 10/23). The harness has no native SL maps, so no projector is excluded;
        # CUBE_POINTONLY=1 restores the earlier point-only residency (diagnostic).
        pointonly = os.environ.get('CUBE_POINTONLY') == '1'
        order = sorted((i for i, l in enumerate(frame.lights) if not pointonly or l.kind == 'point'),
                       key=lambda i: (-importance(frame.lights[i], eye), frame.lights[i].name))
        return order[:n]

    def atlas_for(self, scene, lights, picked, near=NEAR, noignore=False):
        """(atlas float32 (H, 1632), rows {light index: tile row}) for the picked lights; cached per light set."""
        key = (scene.name, near, noignore, tuple((tuple(lights[i].pos), lights[i].ignore, float(lights[i].range)) for i in picked))
        hit = self.atlas_cache.get(key)
        if hit is not None:
            return hit
        atlas = np.ones((ATLAS_H_PER_LIGHT * max(1, len(picked)), ATLAS_W), np.float32)
        rows = {}
        for k, li in enumerate(picked):
            L = lights[li]
            far = float(L.range)
            tiles = light_tiles(scene, L, near, far, self.cache_dir, noignore)
            for f in range(6):
                x, y = (f % 3) * TILE, (2 * k + f // 3) * TILE
                atlas[y:y + TILE, x:x + TILE] = tiles[f]
            rows[li] = 2 * k
        if len(self.atlas_cache) > 64:
            self.atlas_cache.clear()
        self.atlas_cache[key] = (atlas, rows)
        return atlas, rows

    def visibility(self, frame, cfg):
        name, params = cfg
        lights = frame.lights
        plan = self.plans[params['plan']][len(lights)]
        picked = self.resident(frame, params['n'])
        runner = self.runner
        production_filter = runner.filter
        if params['fixedrot']:  # rotation candidate (c): a fixed disk (same context, the filter program swapped for the call)
            if self.filter_fixed is None:
                self.filter_fixed = runner._link('class3/deferred/areaShadowFilterF.glsl', subs=[
                    ('float rot = float(h >> 16)*(6.28318530718/65536.0);', 'float rot = 0.0;')])
            runner.filter = self.filter_fixed
        world = None
        if picked:
            atlas, rows = self.atlas_for(frame.scene, lights, picked, params['near'], params['noignore'])
            channels = []
            for i, L in enumerate(lights):
                if i not in rows:
                    channels.append(None)
                    continue
                far = float(L.range)
                channels.append({'row': rows[i], 'near': near_plane(params['near']), 'far': far,
                                 'r_map': float(min(L.radius if L.radius > 0 else 0.1, 0.25)),
                                 'contact': contact_distance(far) if params['contact'] else 0.0,
                                 'seed': int(L.seed)})
            world = {'atlas': atlas, 'channels': channels}
        try:
            return runner.run(frame.cam, frame.depth32, frame.norm16, lights, plan, world=world, light_bias=params['bias'])
        finally:
            runner.filter = production_filter


Source = CubeSource


def determinism_check():
    """The same pose twice (fresh atlas each time) gives byte-identical output, for every cube config; also the harness hook for CI."""
    import types
    import relight_3d_tasks as T
    sys.modules['__main__'].VisibilitySource = object if not hasattr(sys.modules['__main__'], 'VisibilitySource') else sys.modules['__main__'].VisibilitySource
    src = CubeSource()
    src.prepare({'args': types.SimpleNamespace(shader_ref='WORKTREE'), 'out': str(Path(__file__).resolve().parents[2] / '.tmp' / 'cube_r1' / 'detout')})
    n = 0
    for scene, pose_name in (('B', 'B_camorbit_low_003'), ('A', 'A_orbit_eye_far_000'), ('C', 'C_orbit_005')):
        sc = T.get_scene(scene)
        pose = [p for p in sc.poses if p.id == pose_name][0]
        cam = S.Camera(pose.eye, pose.target, 1920, 1080, pose.vfov)
        g = S.cast_primary(sc, cam)
        _, d32, n16 = S.encode_gbuffer(cam, g)
        frame = types.SimpleNamespace(scene=sc, pose=pose, cam=cam, lights=sc.light_sets[pose.cfg], depth32=d32, norm16=n16)
        for cfg in src.configs({}):
            a = src.visibility(frame, cfg)
            src.atlas_cache.clear()
            b = src.visibility(frame, cfg)
            assert a.tobytes() == b.tobytes(), ('NOT deterministic', pose_name, cfg[0])
            n += 1
    print('[cube] determinism PASS: %d (pose, config) pairs byte-identical on repeat (fresh atlas each run)' % n, flush=True)


if __name__ == '__main__':
    if '--determinism' in sys.argv:
        _m = sys.modules['__main__']
        determinism_check()
