"""Analytic 3D scenes, exact G-buffer ray caster and CPU ground-truth area-light tracer.

Pure numpy, no GL: used by relight_3d_reference.py (worker processes import it too).
Conventions: world is Z-up metres; arrays are indexed [row j][col i] with row 0 at the BOTTOM
of the image (GL readback order); the camera looks down its own -Z like the viewer.
"""
import hashlib
import json
import math
from dataclasses import dataclass, field

import numpy as np

np.seterr(over='ignore', invalid='ignore', divide='ignore')
TRACER_VERSION = 'relight3d-gt-1'
INF = np.float32(1e30)
NEAR, FAR = 0.25, 64.0  # LLCamera DEFAULT_NEAR_PLANE / DEFAULT_FAR_PLANE
VFOV = 60.0


def unit(v):
    v = np.asarray(v, np.float64)
    return v / np.linalg.norm(v)


# --------------------------------------------------------------------------- primitives
@dataclass
class Prim:
    name: str
    kind: str                      # 'obb' | 'sphere' | 'capsule'
    albedo: tuple = (0.7, 0.7, 0.7)
    c: tuple = (0, 0, 0)           # obb/sphere centre
    h: tuple = (1, 1, 1)           # obb half extents
    yaw: float = 0.0               # obb rotation about Z (degrees)
    r: float = 0.0                 # sphere / capsule radius
    a: tuple = (0, 0, 0)           # capsule ends
    b: tuple = (0, 0, 0)
    group: str = ''                # key-blocker group for on/off-screen reporting

    def rot(self):
        t = math.radians(self.yaw)
        return np.array([[math.cos(t), -math.sin(t), 0], [math.sin(t), math.cos(t), 0], [0, 0, 1]])

    def aabb(self):
        if self.kind == 'obb':
            ext = np.abs(self.rot()) @ np.array(self.h)
            c = np.array(self.c)
            return c - ext, c + ext
        if self.kind == 'sphere':
            c = np.array(self.c)
            return c - self.r, c + self.r
        a, b = np.array(self.a), np.array(self.b)
        return np.minimum(a, b) - self.r, np.maximum(a, b) + self.r

    def spec(self):
        return [self.name, self.kind, list(map(float, self.c)), list(map(float, self.h)), self.yaw, self.r,
                list(map(float, self.a)), list(map(float, self.b))]


def isect(p, o, d, eps=1e-5):
    """Nearest positive ray parameter, float32 inf where missed. o,d: (N,3) (d unit)."""
    if p.kind == 'obb':
        R = p.rot().astype(o.dtype)
        ol = (o - np.asarray(p.c, o.dtype)) @ R
        dl = d @ R
        dl = np.where(np.abs(dl) < 1e-12, np.asarray(1e-12, o.dtype), dl)
        inv = 1.0 / dl
        h = np.asarray(p.h, o.dtype)
        t1 = (-h - ol) * inv
        t2 = (h - ol) * inv
        tn = np.minimum(t1, t2).max(1)
        tf = np.maximum(t1, t2).min(1)
        t = np.where(tn > eps, tn, tf)
        hit = (tf >= tn) & (tf > eps)
        return np.where(hit, t, INF)
    if p.kind == 'sphere':
        return _sphere(np.asarray(p.c, o.dtype), p.r, o, d, eps)
    return _capsule(np.asarray(p.a, o.dtype), np.asarray(p.b, o.dtype), p.r, o, d, eps)


def _sphere(c, r, o, d, eps):
    oc = o - c
    b = (oc * d).sum(1)
    cc = (oc * oc).sum(1) - r * r
    disc = b * b - cc
    sq = np.sqrt(np.maximum(disc, 0))
    t = -b - sq
    t = np.where(t > eps, t, -b + sq)
    return np.where((disc >= 0) & (t > eps), t, INF).astype(o.dtype)


def _capsule(a, b, r, o, d, eps):
    ba = b - a
    oa = o - a
    baba = float(ba @ ba)
    bard = d @ ba
    baoa = oa @ ba
    rd = (d * 1.0)
    k2 = baba - bard * bard
    k1 = baba * (oa * rd).sum(1) - baoa * bard
    k0 = baba * (oa * oa).sum(1) - baoa * baoa - r * r * baba
    k2s = np.where(np.abs(k2) < 1e-12, 1e-12, k2)
    h = k1 * k1 - k2s * k0
    sq = np.sqrt(np.maximum(h, 0))
    t = (-k1 - sq) / k2s
    y = baoa + t * bard
    body = (h >= 0) & (t > eps) & (y > 0) & (y < baba)
    # end caps: choose the near end sphere by y sign
    tcap_a = _sphere(a, r, o, d, eps)
    tcap_b = _sphere(b, r, o, d, eps)
    # a cap sphere only counts where the hit point lies beyond that end
    pa = o + d * tcap_a[:, None]
    pb = o + d * tcap_b[:, None]
    ok_a = ((pa - a) @ ba) <= 0
    ok_b = ((pb - b) @ ba) >= 0
    tcap = np.minimum(np.where(ok_a, tcap_a, INF), np.where(ok_b, tcap_b, INF))
    tb = np.where(body, t, INF).astype(o.dtype)
    return np.minimum(tb, tcap)


def normal_at(p, pt):
    """Outward geometric normal of prim p at surface points pt (N,3)."""
    if p.kind == 'obb':
        R = p.rot()
        loc = (pt - np.array(p.c)) @ R
        q = loc / np.array(p.h)
        ax = np.abs(q).argmax(1)
        n = np.zeros_like(loc)
        n[np.arange(len(loc)), ax] = np.sign(q[np.arange(len(loc)), ax])
        return n @ R.T
    if p.kind == 'sphere':
        n = pt - np.array(p.c)
        return n / np.linalg.norm(n, axis=1, keepdims=True)
    a, b = np.array(p.a, np.float64), np.array(p.b, np.float64)
    ba = b - a
    s = np.clip(((pt - a) @ ba) / (ba @ ba), 0, 1)
    n = pt - (a + s[:, None] * ba)
    return n / np.linalg.norm(n, axis=1, keepdims=True)


# --------------------------------------------------------------------------- lights
@dataclass
class Light:
    name: str
    pos: tuple
    radius: float                  # source disk radius (m) used by shader and ground truth
    range: float                   # lighting range (m); receivers beyond it get no light
    kind: str = 'point'            # 'point' | 'projector'
    dir: tuple = (0, 0, -1)        # projector axis
    cone: float = 35.0             # projector half angle (deg)
    ignore: str = ''               # prim that houses the light: never shadows it (ground truth)
    seed: int = 1

    def spec(self):
        return [self.name, list(map(float, self.pos)), self.radius, self.range, self.kind,
                list(map(float, self.dir)), self.cone, self.ignore, self.seed]


def fnv_seed(text):
    h = 2166136261
    for b in text.encode():
        h = ((h ^ b) * 16777619) & 0xffffffff
    x = h  # ALAreaShadow::hash avalanche
    x ^= x >> 16
    x = (x * 0x7feb352d) & 0xffffffff
    x ^= x >> 15
    x = (x * 0x846ca68b) & 0xffffffff
    return x ^ (x >> 16)


def unshadowed(light, P, N):
    """CPU unshadowed lighting + candidate mask for surface points. P,N (n,3)."""
    lv = np.array(light.pos) - P
    d = np.linalg.norm(lv, axis=1)
    l = lv / np.maximum(d, 1e-9)[:, None]
    nl = (N * l).sum(1)
    fall = np.clip(1.0 - d / light.range, 0, 1) ** 2
    cone = np.ones_like(d)
    if light.kind == 'projector':
        cs = -(l @ unit(light.dir))
        lo, hi = math.cos(math.radians(light.cone)), math.cos(math.radians(light.cone * 0.7))
        x = np.clip((cs - lo) / (hi - lo), 0, 1)
        cone = x * x * (3 - 2 * x)
    L0 = np.maximum(nl, 0) * fall * cone
    cand = (nl > 0.05) & (d < light.range) & (cone > 0.02)
    return L0.astype(np.float32), cand


# --------------------------------------------------------------------------- scenes
@dataclass
class Pose:
    id: str
    scene: str
    group: str                     # adjacency sequence (poses are neighbours in idx order)
    idx: int
    eye: tuple
    target: tuple
    cfg: int = 0                   # light configuration index
    tag: str = 'orbit'             # orbit | offscreen | hidden | dolly | user ...
    vfov: float = VFOV
    note: str = ''

    def spec(self):
        return [self.id, self.scene, self.group, self.idx, list(map(float, self.eye)),
                list(map(float, self.target)), self.cfg, self.vfov]


@dataclass
class Scene:
    name: str
    prims: list
    light_sets: list               # list of list[Light]
    poses: list
    groups: dict                   # key blocker group -> [prim names]
    probes: np.ndarray             # (K,3) floor probe points (z-up normal)
    probe_normal: tuple = (0, 0, 1)
    pool: dict = field(default_factory=dict)  # lamp-pool check: {'light':int,'surf':[names],'radius':m}
    description: str = ''

    def prim_index(self, name):
        for i, p in enumerate(self.prims):
            if p.name == name:
                return i
        return -1

    def signature(self):
        blob = json.dumps([[p.spec() for p in self.prims], [[l.spec() for l in ls] for ls in self.light_sets],
                           self.probes.round(4).tolist(), TRACER_VERSION], sort_keys=True)
        return hashlib.sha1(blob.encode()).hexdigest()[:16]


def _floor_probes(x0, x1, y0, y1, step, z=0.0):
    xs = np.arange(x0 + step / 2, x1, step)
    ys = np.arange(y0 + step / 2, y1, step)
    gx, gy = np.meshgrid(xs, ys)
    return np.stack([gx.ravel(), gy.ravel(), np.full(gx.size, z)], 1)


def _orbit(scene, group, centre, radius, height, count, start=0.0, span=360.0, cfg=0, tag='orbit', aim_z=None, idx0=0):
    out = []
    for k in range(count):
        az = math.radians(start + span * k / count)
        eye = (centre[0] + radius * math.sin(az), centre[1] - radius * math.cos(az), height)
        tgt = (centre[0], centre[1], centre[2] if aim_z is None else aim_z)
        out.append(Pose('%s_%s_%03d' % (scene, group, idx0 + k), scene, group, idx0 + k, eye, tgt, cfg, tag))
    return out


def _railing(prefix, pts, group, rails=(0.5, 1.0), r=0.02, post_h=1.0):
    """Two-rail railing through polyline pts (x,y); posts at each vertex of pts."""
    prims = []
    for k, (x, y) in enumerate(pts):
        prims.append(Prim('%s_post%d' % (prefix, k), 'capsule', (0.1, 0.1, 0.12), r=r,
                          a=(x, y, r), b=(x, y, post_h - r + 0.02), group=group))
    for s, (p0, p1) in enumerate(zip(pts[:-1], pts[1:])):
        for z in rails:
            prims.append(Prim('%s_rail%d_%d' % (prefix, s, int(z * 10)), 'capsule', (0.1, 0.1, 0.12), r=r,
                              a=(p0[0], p0[1], z), b=(p1[0], p1[1], z), group=group))
    return prims


R_FAIL_POSES = [
    # F1 off-screen: floor shadow of box_float is in frame, the floating box itself is above the frame
    ('offscreen_up', (-3.2, 2.2, 2.3), (-3.8, 3.4, 0.0), 35.0, 'offscreen', 'floating box above the frame'),
    # F2 off-screen: camera faces the north wall; every blocker is behind the camera
    ('behind', (-0.2, 4.6, 1.5), (-0.2, 6.0, 1.0), 50.0, 'offscreen', 'all blockers behind the camera'),
    # F3 hidden: box_floor stands between the camera and the sphere/actor whose shadows are on the visible floor
    ('hidden', (2.4, 3.1, 0.9), (-1.6, -1.4, 0.2), 50.0, 'hidden', 'blocker hidden behind box_floor'),
]


def scene_room():
    """Room corner + boxes + sphere + actor capsule + 4 cm railing; point and projector light."""
    pr = [
        Prim('floor', 'obb', (0.55, 0.52, 0.48), c=(1.5, -1.5, -0.15), h=(7.5, 7.5, 0.15)),
        Prim('wall_n', 'obb', (0.72, 0.70, 0.66), c=(1.5, 6.2, 1.75), h=(7.5, 0.2, 1.75)),
        Prim('wall_w', 'obb', (0.66, 0.70, 0.72), c=(-6.2, -1.5, 1.75), h=(0.2, 7.7, 1.75)),
        Prim('box_floor', 'obb', (0.75, 0.45, 0.25), c=(0.9, 1.4, 0.5), h=(0.5, 0.5, 0.5), yaw=20, group='box_floor'),
        Prim('box_float', 'obb', (0.3, 0.55, 0.75), c=(-1.8, 1.5, 1.6), h=(0.35, 0.35, 0.35), yaw=35, group='box_float'),
        Prim('box_small', 'obb', (0.45, 0.7, 0.35), c=(2.6, -0.2, 0.2), h=(0.2, 0.3, 0.2), yaw=-15, group='box_small'),
        Prim('sphere', 'sphere', (0.8, 0.8, 0.3), c=(-1.6, -1.4, 0.4), r=0.4, group='sphere'),
        Prim('actor', 'capsule', (0.8, 0.55, 0.5), r=0.25, a=(-0.2, 0.0, 0.25), b=(-0.2, 0.0, 1.55), group='actor'),
    ]
    pr += [Prim('rail_post0', 'capsule', (0.1, 0.1, 0.12), r=0.02, a=(-4.5, -3.2, 0.02), b=(-4.5, -3.2, 0.92), group='railing'),
           Prim('rail_post1', 'capsule', (0.1, 0.1, 0.12), r=0.02, a=(-1.5, -3.2, 0.02), b=(-1.5, -3.2, 0.92), group='railing'),
           Prim('rail_bar', 'capsule', (0.1, 0.1, 0.12), r=0.02, a=(-4.5, -3.2, 0.9), b=(-1.5, -3.2, 0.9), group='railing')]
    lights_a = [Light('point', (0.6, -0.6, 3.3), 0.1, 12.0, seed=fnv_seed('R:point')),
                Light('projector', (-3.4, -3.2, 3.8), 0.15, 14.0, 'projector', dir=(0.5, 0.5, -0.7), cone=40.0,
                      seed=fnv_seed('R:projector'))]
    sc = Scene('R', pr, [lights_a], [], {}, _floor_probes(-5.5, 5.5, -5.5, 5.5, 0.1))
    sc.groups = {'box_floor': ['box_floor'], 'box_float': ['box_float'], 'box_small': ['box_small'], 'sphere': ['sphere'], 'actor': ['actor'],
                 'railing': ['rail_post0', 'rail_post1', 'rail_bar']}
    centre = (-0.2, 0.0, 0.9)
    poses = []
    k = 0
    for height, rad, tag in [(1.6, 3.8, 'orbit'), (1.6, 5.2, 'orbit'), (4.2, 3.8, 'orbit'), (4.2, 5.2, 'orbit')]:
        for az in range(6):
            a = math.radians(az * 60.0 + (30.0 if height > 3 else 0.0))
            eye = (centre[0] + rad * math.sin(a), centre[1] - rad * math.cos(a), height)
            poses.append(Pose('R_orbit_%03d' % k, 'R', 'orbit_h%d_d%d' % (int(height * 10), int(rad * 10)), az,
                              eye, centre, 0, tag))
            k += 1
    # Expected screen-space failures: blocker OFF-SCREEN or HIDDEN behind another object (one group each, so no
    # adjacency pairs are formed between these unrelated views).
    for k, (nm, eye, tgt, vf, tag, note) in enumerate(R_FAIL_POSES):
        poses.append(Pose('R_fail_%s' % nm, 'R', 'fail%d' % k, 0, eye, tgt, 0, tag, vfov=vf, note=note))
    sc.poses = poses
    sc.description = 'room corner, 3 boxes, sphere, actor capsule, 4 cm railing; point + projector light'
    return sc


def scene_railing():
    """A. Balcony railing (two 4 cm round rails at 0.5/1.0 m, posts every 1.2 m) on concrete, light off to the side."""
    xs = [-3.6 + 1.2 * k for k in range(7)]
    pr = [Prim('floor', 'obb', (0.5, 0.5, 0.48), c=(0, 0, -0.15), h=(6.0, 5.0, 0.15))]
    pr += _railing('rail', [(x, 0.0) for x in xs], 'railing')
    light = Light('point', (-1.0, -1.7, 1.5), 0.1, 12.0, seed=fnv_seed('A:point'))
    sc = Scene('A', pr, [[light]], [], {'railing': [p.name for p in pr[1:]]}, _floor_probes(-4.5, 4.5, 0.05, 4.9, 0.1))
    centre = (0.0, 1.5, 0.3)
    poses = []
    for dist, dtag in [(4.5, 'near'), (7.5, 'far')]:
        for height, htag in [(1.7, 'eye'), (3.5, 'mid'), (6.0, 'high')]:
            poses += _orbit('A', 'orbit_%s_%s' % (htag, dtag), centre, dist, height, 12)
    # _orbit ids are unique per group already
    sc.poses = poses
    sc.description = 'balcony railing, 7 posts / 2 rails, floor shadows from a side point light'
    return sc


def scene_railing_objects():
    """A2. Scene A (the same railing, light and orbit poses) plus RECEIVER objects behind the railing, on the far side from the light, so the
    railing's shadow falls across them: a 0.8 m crate whose near face is 1.0 m behind the railing (the shadow on a vertical face and wrapping
    from the floor onto the box) and a 0.5 m sphere (a curved receiver) beside it. Both cast their own shadows onto the floor as well.
    The key blocker group is still the railing only, so the pose gating and the metrics are scene A's."""
    sc = scene_railing()
    sc.name = 'A2'
    sc.prims += [Prim('crate', 'obb', (0.62, 0.46, 0.30), c=(0.8, 1.5, 0.4), h=(0.6, 0.5, 0.4), yaw=12),
                 Prim('ball', 'sphere', (0.82, 0.78, 0.66), c=(-1.1, 1.45, 0.25), r=0.25)]
    sc.light_sets = [[Light('point', (-1.0, -1.7, 1.5), 0.1, 12.0, seed=fnv_seed('A2:point'))]]
    sc.poses = [Pose(p.id.replace('A_', 'A2_', 1), 'A2', p.group, p.idx, p.eye, p.target, p.cfg, p.tag) for p in sc.poses]
    sc.description = 'balcony railing + a crate and a sphere behind it (the railing shadow falls across them), side point light'
    return sc


def scene_sphere():
    """B. 0.5 m sphere on a floor; light flies a circle (8 angles x 3 heights + 12 synced) while the camera orbits."""
    pr = [Prim('floor', 'obb', (0.5, 0.5, 0.48), c=(0, 0, -0.15), h=(8.0, 8.0, 0.15)),
          Prim('sphere', 'sphere', (0.85, 0.8, 0.7), c=(0, 0, 0.25), r=0.25, group='sphere')]
    sets = []
    for hi, h in enumerate((0.9, 1.6, 2.4)):
        for ti in range(8):
            t = math.radians(ti * 45.0)
            sets.append([Light('point', (2.0 * math.cos(t), 2.0 * math.sin(t), h), 0.1, 10.0,
                               seed=fnv_seed('B:point'))])
    for ti in range(12):
        t = math.radians(ti * 30.0)
        sets.append([Light('point', (2.0 * math.cos(t), 2.0 * math.sin(t), 1.6), 0.1, 10.0, seed=fnv_seed('B:point'))])
    sc = Scene('B', pr, sets, [], {'sphere': ['sphere']}, _floor_probes(-2.8, 2.8, -2.8, 2.8, 0.07))
    centre = (0.0, 0.0, 0.25)
    poses = []
    for ti in range(8):  # camera fixed, light moves
        poses.append(Pose('B_lightsweep_h09_%03d' % ti, 'B', 'lightsweep_h09', ti, (0, -3.4, 1.8), centre, ti, 'lightsweep'))
    for ti in range(8):
        poses.append(Pose('B_lightsweep_h16_%03d' % ti, 'B', 'lightsweep_h16', ti, (0, -3.4, 1.8), centre, 8 + ti, 'lightsweep'))
    for ti in range(8):
        poses.append(Pose('B_lightsweep_h24_%03d' % ti, 'B', 'lightsweep_h24', ti, (0, -3.4, 1.8), centre, 16 + ti, 'lightsweep'))
    fixed = 8 + 1  # light fixed at angle 45 deg, height 1.6
    for height, tag in [(1.3, 'low'), (3.2, 'high')]:
        poses += _orbit('B', 'camorbit_%s' % tag, centre, 3.2, height, 12, cfg=fixed, tag='camorbit')
    for k in range(12):  # both move
        a = math.radians(k * 30.0)
        poses.append(Pose('B_both_%03d' % k, 'B', 'both', k, (3.4 * math.sin(a), -3.4 * math.cos(a), 2.2), centre, 24 + k, 'both'))
    sc.poses = poses
    sc.description = 'sphere on floor; moving light circle; camera orbit'
    return sc


def scene_balcony():
    """C. Simplified analytic copy of the user's night balcony."""
    P = Prim
    pr = [
        P('floor', 'obb', (0.5, 0.5, 0.46), c=(0, 2.4, -0.15), h=(3.0, 2.4, 0.15)),
        P('ground', 'obb', (0.25, 0.27, 0.3), c=(0, 0, -6.15), h=(30, 30, 0.15)),
        P('wall_back', 'obb', (0.55, 0.57, 0.52), c=(0, 5.0, 1.75), h=(3.4, 0.2, 1.75)),
        P('wall_side', 'obb', (0.5, 0.56, 0.6), c=(3.2, 2.6, 1.75), h=(0.2, 2.6, 1.75)),
        P('door', 'obb', (0.32, 0.22, 0.14), c=(0.8, 4.77, 1.05), h=(0.45, 0.03, 1.05)),
        P('lamp_shade', 'obb', (0.2, 0.22, 0.3), c=(0.8, 4.66, 2.45), h=(0.15, 0.12, 0.10), group='lamp'),
        P('crate1', 'obb', (0.55, 0.42, 0.25), c=(-2.4, 4.2, 0.4), h=(0.4, 0.4, 0.4), yaw=8, group='crates'),
        P('crate2', 'obb', (0.5, 0.4, 0.25), c=(-1.55, 4.25, 0.35), h=(0.35, 0.35, 0.35), yaw=-5, group='crates'),
        P('crate3', 'obb', (0.52, 0.4, 0.22), c=(-2.35, 4.22, 1.2), h=(0.38, 0.38, 0.4), yaw=15, group='crates'),
        P('crate4', 'obb', (0.5, 0.42, 0.28), c=(-1.5, 4.3, 1.0), h=(0.3, 0.3, 0.3), yaw=22, group='crates'),
        P('pipe_v', 'capsule', (0.3, 0.3, 0.34), r=0.08, a=(-0.75, 4.5, 0.1), b=(-0.75, 4.5, 1.0), group='pipe'),
        P('pipe_h', 'capsule', (0.3, 0.3, 0.34), r=0.08, a=(-0.75, 4.5, 1.0), b=(-0.75, 4.76, 1.0), group='pipe'),
    ]
    pr += _railing('rf', [(-3.0 + 1.2 * k, 0.0) for k in range(6)], 'railing')
    pr += _railing('rl', [(-3.0, 1.2 * k) for k in range(5)], 'railing')
    pr = pr
    lamp = Light('lamp', (0.8, 4.66, 2.45), 0.05, 7.0, ignore='lamp_shade', seed=fnv_seed('C:lamp'))
    side = Light('side', (1.0, -1.8, 1.7), 0.1, 10.0, seed=fnv_seed('C:side'))
    sc = Scene('C', pr, [[lamp, side]], [], {'railing': [p.name for p in pr if p.group == 'railing'],
                                              'crates': ['crate1', 'crate2', 'crate3', 'crate4'],
                                              'pipe': ['pipe_v', 'pipe_h']},
               _floor_probes(-3.0, 3.0, 0.1, 4.7, 0.1),
               pool={'light': 0, 'surf': ['wall_back', 'door'], 'radius': 1.8})
    # user's view: eye 3.6 m up, ~42 deg down toward the door; orbit from -75 to +15 deg around that pivot (k=10 is the user view)
    pivot = np.array([0.6, 3.2])
    eye0 = np.array([2.0, -0.5])
    rel = eye0 - pivot
    base = math.atan2(rel[1], rel[0])
    rad = float(np.linalg.norm(rel))
    poses = []
    for k in range(13):
        a = base + math.radians(-75 + 7.5 * k)
        eye = (pivot[0] + rad * math.cos(a), pivot[1] + rad * math.sin(a), 3.6)
        poses.append(Pose('C_orbit_%03d' % k, 'C', 'orbit', k, eye, (pivot[0], pivot[1], 0.0), 0,
                          'user' if k == 10 else 'orbit'))
    for k in range(9):
        x = -2.6 + 5.2 * k / 8
        poses.append(Pose('C_dolly_%03d' % k, 'C', 'dolly', k, (x, -1.2, 1.6), (0.2, 3.0, 0.5), 0, 'dolly'))
    sc.poses = poses
    sc.description = 'night balcony replica: slab with drop-off, door + wall lamp in shade, railing, crates, pipe, side wall'
    return sc


SCENES = {'R': scene_room, 'A': scene_railing, 'A2': scene_railing_objects, 'B': scene_sphere, 'C': scene_balcony}


# --------------------------------------------------------------------------- camera / G-buffer
class Camera:
    def __init__(self, eye, target, W, H, vfov=VFOV, near=NEAR, far=FAR):
        self.eye = np.array(eye, np.float64)
        self.f = unit(np.array(target) - self.eye)
        self.r = unit(np.cross(self.f, [0, 0, 1.0]))
        self.u = np.cross(self.r, self.f)
        self.W, self.H, self.near, self.far = W, H, near, far
        self.tan = math.tan(math.radians(vfov) / 2)
        self.aspect = W / H
        V = np.eye(4)
        V[0, :3], V[1, :3], V[2, :3] = self.r, self.u, -self.f
        V[:3, 3] = -V[:3, :3] @ self.eye
        P = np.zeros((4, 4))
        P[0, 0] = 1 / (self.aspect * self.tan)
        P[1, 1] = 1 / self.tan
        P[2, 2] = -(far + near) / (far - near)
        P[2, 3] = -2 * far * near / (far - near)
        P[3, 2] = -1
        self.view, self.proj = V, P

    def rays(self, j0, j1, i0=0, i1=None, stride=1, offset=0.5):
        """Rays for pixel rows j0..j1 (bottom-up) at pixel centres."""
        i1 = self.W if i1 is None else i1
        ii = (np.arange(i0, i1) + offset)
        jj = (np.arange(j0, j1) + offset)
        x = (ii / self.W * 2 - 1) * self.tan * self.aspect
        y = (jj / self.H * 2 - 1) * self.tan
        d = self.f[None, None, :] + x[None, :, None] * self.r[None, None, :] + y[:, None, None] * self.u[None, None, :]
        d /= np.linalg.norm(d, axis=2, keepdims=True)
        return d

    def project(self, pts):
        """World points (N,3) -> (px,py float pixel coords [0,W)x[0,H) bottom-up, zf)."""
        c = pts - self.eye
        zf = c @ self.f
        zs = np.where(zf > 1e-6, zf, 1e-6)
        nx = (c @ self.r) / (zs * self.tan * self.aspect)
        ny = (c @ self.u) / (zs * self.tan)
        return (nx * 0.5 + 0.5) * self.W, (ny * 0.5 + 0.5) * self.H, zf

    def depth24(self, zf):
        """Window depth for linear depth zf, quantised to the 24-bit viewer depth buffer."""
        ndc = (self.far + self.near) / (self.far - self.near) - 2 * self.far * self.near / ((self.far - self.near) * zf)
        d = np.clip(ndc * 0.5 + 0.5, 0, 1)
        return np.round(d * 16777215.0).astype(np.uint32)


def oct_encode(n):
    """Exactly encodeNormal() of globalF.glsl: n (N,3) view-space unit -> (N,2) in [0,1]."""
    n = n / np.abs(n).sum(1, keepdims=True)
    xy = n[:, :2].copy()
    wrap = n[:, 2] < 0
    ox = (1 - np.abs(xy[:, 1])) * np.where(xy[:, 0] >= 0, 1.0, -1.0)
    oy = (1 - np.abs(xy[:, 0])) * np.where(xy[:, 1] >= 0, 1.0, -1.0)
    xy[wrap, 0], xy[wrap, 1] = ox[wrap], oy[wrap]
    return xy * 0.5 + 0.5


def oct_decode(rg):
    """decodeNormal() of globalF.glsl (numpy port), rg (N,2) in [0,1]."""
    f = rg * 2 - 1
    n = np.stack([f[:, 0], f[:, 1], 1 - np.abs(f[:, 0]) - np.abs(f[:, 1])], 1)
    t = np.clip(-n[:, 2], 0, 1)
    n[:, 0] += np.where(n[:, 0] >= 0, -t, t)
    n[:, 1] += np.where(n[:, 1] >= 0, -t, t)
    return n / np.linalg.norm(n, axis=1, keepdims=True)


def _prim_screen_bounds(cam, prim):
    """Conservative pixel bounds of a prim's AABB, or None when any corner is at/behind the camera plane."""
    lo, hi = prim.aabb()
    corners = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
    px, py, zf = cam.project(corners)
    if np.any(zf <= cam.near * 0.5):
        return None
    return px.min(), px.max(), py.min(), py.max()


def cast_primary(scene, cam, tile=(256, 96)):
    """Exact per-pixel nearest hit. Returns dict of full-resolution arrays."""
    H, W = cam.H, cam.W
    zf = np.full((H, W), np.inf, np.float32)
    surf = np.full((H, W), 255, np.uint8)
    pos = np.zeros((H, W, 3), np.float32)
    nrm = np.zeros((H, W, 3), np.float32)
    eye = cam.eye
    bounds = [_prim_screen_bounds(cam, p) for p in scene.prims]
    tw, th = tile
    for j0 in range(0, H, th):
        j1 = min(H, j0 + th)
        for i0 in range(0, W, tw):
            i1 = min(W, i0 + tw)
            use = [pi for pi, b in enumerate(bounds)
                   if b is None or not (b[0] > i1 + 1 or b[1] < i0 - 1 or b[2] > j1 + 1 or b[3] < j0 - 1)]
            d = cam.rays(j0, j1, i0, i1).reshape(-1, 3)
            o = np.broadcast_to(eye, d.shape).copy()
            best = np.full(len(d), np.inf)
            bi = np.full(len(d), -1, np.int32)
            for pi in use:
                t = isect(scene.prims[pi], o, d).astype(np.float64)
                t[t >= 1e29] = np.inf
                m = t < best
                best = np.where(m, t, best)
                bi = np.where(m, pi, bi)
            hit = bi >= 0
            pt = eye + d * np.where(hit, best, 0)[:, None]
            n = np.zeros_like(pt)
            for pi in set(bi[hit].tolist()):
                m = bi == pi
                nn = normal_at(scene.prims[pi], pt[m])
                flip = (nn * d[m]).sum(1) > 0
                n[m] = np.where(flip[:, None], -nn, nn)
            zfl = np.where(hit, (pt - eye) @ cam.f, np.inf)
            shp = (j1 - j0, i1 - i0)
            zf[j0:j1, i0:i1] = zfl.reshape(shp)
            surf[j0:j1, i0:i1] = np.where(hit, bi, 255).reshape(shp).astype(np.uint8)
            pos[j0:j1, i0:i1] = pt.reshape(shp + (3,))
            nrm[j0:j1, i0:i1] = n.reshape(shp + (3,))
    return {'zf': zf, 'surf': surf, 'pos': pos, 'nrm': nrm}


def encode_gbuffer(cam, g):
    """Viewer-format G-buffer from exact casts: 24-bit depth + RGBA16 octahedral normals."""
    H, W = cam.H, cam.W
    sky = ~np.isfinite(g['zf'])
    depth = cam.depth24(np.where(sky, 1.0, g['zf']))
    depth[sky] = 16777215
    nv = g['nrm'].reshape(-1, 3).astype(np.float64) @ cam.view[:3, :3].T
    nv /= np.maximum(np.linalg.norm(nv, axis=1, keepdims=True), 1e-9)
    enc = oct_encode(nv)
    norm16 = np.zeros((H * W, 4), np.uint16)
    norm16[:, :2] = np.round(np.clip(enc, 0, 1) * 65535).astype(np.uint16)
    norm16 = norm16.reshape(H, W, 4)
    norm16[sky] = 0
    norm16[sky, 0] = norm16[sky, 1] = 32768
    # depth word for GL_DEPTH_COMPONENT24 + GL_UNSIGNED_INT upload: scale 24-bit unorm to 32-bit unorm
    depth32 = np.round(depth.astype(np.float64) * (4294967295.0 / 16777215.0)).astype(np.uint32)
    return depth, depth32, norm16


# --------------------------------------------------------------------------- ground-truth tracer
GT_REASON_ON, GT_REASON_OFF, GT_REASON_HIDDEN, GT_REASON_ABSENT = 0, 1, 2, 3


def _disk_samples(S):
    i = np.arange(S, dtype=np.float64)
    r = np.sqrt((i + 0.5) / S)
    th = i * 2.399963229728653
    return r, th


def _hash_u32(x, seed):
    x = (x.astype(np.uint64) * np.uint64(2654435761) + np.uint64(seed)) & np.uint64(0xffffffff)
    x ^= x >> np.uint64(16)
    x = (x * np.uint64(0x7feb352d)) & np.uint64(0xffffffff)
    x ^= x >> np.uint64(15)
    x = (x * np.uint64(0x846ca68b)) & np.uint64(0xffffffff)
    return (x ^ (x >> np.uint64(16))).astype(np.uint32)


def trace_light(scene, light, P, N, pix_id, S=256, cam=None, zbuf=None, thickness=0.35, chunk_rays=65536):
    """True area-light visibility of surface points (stratified disk of `S` rays per point).

    Returns dict: vis (n,), nblk (n,), reasons (n,4) counts [on, off-screen, hidden, absent] of the blocked
    rays' first hit versus the G-buffer, tmean (n,) mean receiver-to-blocker distance of the blocked rays (INF if lit).
    The light's housing prim (light.ignore) never shadows it. Segments end on the disk sample.
    """
    n = len(P)
    vis = np.ones(n, np.float32)
    nblk = np.zeros(n, np.int32)
    reasons = np.zeros((n, 4), np.uint16)
    tmean = np.full(n, INF, np.float32)
    if n == 0:
        return {'vis': vis, 'nblk': nblk, 'reasons': reasons, 'tmean': tmean}
    r, th = _disk_samples(S)
    ign = scene.prim_index(light.ignore) if light.ignore else -1
    prims = [(i, p, p.aabb()) for i, p in enumerate(scene.prims) if i != ign]
    Lc = np.array(light.pos, np.float32)
    per = max(1, chunk_rays // S)
    for c0 in range(0, n, per):
        c1 = min(n, c0 + per)
        Pc = P[c0:c1].astype(np.float32)
        Nc = N[c0:c1].astype(np.float32)
        m = c1 - c0
        o = Pc + Nc * np.float32(1e-3)
        dl = Lc - o
        dl /= np.linalg.norm(dl, axis=1, keepdims=True)
        helper = np.where(np.abs(dl[:, 2:3]) < 0.99, np.array([[0, 0, 1]], np.float32), np.array([[0, 1, 0]], np.float32))
        tg = np.cross(dl, helper)
        tg /= np.linalg.norm(tg, axis=1, keepdims=True)
        bt = np.cross(dl, tg)
        rot = _hash_u32(pix_id[c0:c1], int(light.seed)).astype(np.float64) / 4294967296.0 * 2 * math.pi
        ang = th[None, :] + rot[:, None]
        ox = (light.radius * r[None, :] * np.cos(ang)).astype(np.float32)
        oy = (light.radius * r[None, :] * np.sin(ang)).astype(np.float32)
        tgt = Lc + ox[:, :, None] * tg[:, None, :] + oy[:, :, None] * bt[:, None, :]  # (m,S,3)
        dv = tgt - o[:, None, :]
        tmax = np.linalg.norm(dv, axis=2)
        d = (dv / tmax[:, :, None]).reshape(-1, 3)
        oo = np.broadcast_to(o[:, None, :], (m, S, 3)).reshape(-1, 3)
        tmx = tmax.reshape(-1) - np.float32(1e-3)
        lo = np.minimum(o.min(0), tgt.reshape(-1, 3).min(0)) - 1e-3
        hi = np.maximum(o.max(0), tgt.reshape(-1, 3).max(0)) + 1e-3
        best = np.full(len(d), INF, np.float32)
        for pi, p, (blo, bhi) in prims:
            if np.any(blo > hi) or np.any(bhi < lo):
                continue
            t = isect(p, oo, d)
            best = np.minimum(best, np.where(t < tmx, t, INF))
        blocked = best < INF
        b2 = blocked.reshape(m, S)
        nb = b2.sum(1)
        nblk[c0:c1] = nb
        vis[c0:c1] = 1.0 - nb / float(S)
        tb = np.where(b2, best.reshape(m, S), 0).sum(1) / np.maximum(nb, 1)
        tmean[c0:c1] = np.where(nb > 0, tb, INF)
        if cam is not None and blocked.any():
            H = oo[blocked] + d[blocked] * best[blocked][:, None]
            px, py, zh = cam.project(H.astype(np.float64))
            off = (zh <= cam.near) | (px < 0) | (px >= cam.W) | (py < 0) | (py >= cam.H)
            ii = np.clip(px.astype(np.int64), 0, cam.W - 1)
            jj = np.clip(py.astype(np.int64), 0, cam.H - 1)
            zb = zbuf[jj, ii].astype(np.float64)
            tol = max(thickness, 0.0) + 0.01 * zh
            diff = zh - zb
            code = np.where(off, GT_REASON_OFF,
                            np.where(diff > tol, GT_REASON_HIDDEN, np.where(diff < -tol, GT_REASON_ABSENT, GT_REASON_ON)))
            rows = np.nonzero(blocked)[0] // S
            for k in range(4):
                np.add.at(reasons[c0:c1, k], rows[code == k], 1)
    return {'vis': vis, 'nblk': nblk, 'reasons': reasons, 'tmean': tmean}


def tile_order(gi, gj, tile=16):
    """Stable spatially coherent ordering of grid pixels so ray bundles are compact."""
    key = ((gj // tile) * 4096 + (gi // tile)) * 4096 + (gj % tile) * 64 + (gi % tile)
    return np.argsort(key, kind='stable')
