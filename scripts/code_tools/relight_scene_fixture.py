"""Synthetic G-buffers for the Relight soft-shadow GPU gates: analytic planes seen by a perspective
camera with any yaw/pitch/roll, plus the geometric oracles for them. CPU only (numpy).

A scene is a list of surfaces (infinite planes or rectangles). Rays are cast per pixel centre, the
nearest surface wins, and the depth/normal buffers are written exactly as the viewer would store
them (window depth from the projection, octahedral view-space normals). The oracles never look at
the shader: they intersect rays with the same surfaces.
"""


def camera(np, eye, yaw, pitch, roll, w, h, fov_deg=60.0, near=.1, far=80.0, jitter=(0.0, 0.0)):
    """View/projection for a camera at `eye` (world) looking along yaw (about +Y) and pitch, rolled."""
    yaw, pitch, roll = np.radians([yaw, pitch, roll])
    forward = np.array([np.sin(yaw) * np.cos(pitch), np.sin(pitch), -np.cos(yaw) * np.cos(pitch)])
    right0 = np.cross(forward, [0, 1.0, 0])
    right0 /= np.linalg.norm(right0)
    up0 = np.cross(right0, forward)
    right = np.cos(roll) * right0 + np.sin(roll) * up0
    up = -np.sin(roll) * right0 + np.cos(roll) * up0
    rotation = np.stack([right, up, -forward])
    view = np.eye(4)
    view[:3, :3] = rotation
    view[:3, 3] = -rotation @ np.asarray(eye, dtype=float)
    focal = 1.0 / np.tan(np.radians(fov_deg) / 2)
    proj = np.zeros((4, 4))
    proj[0, 0] = focal * h / w
    proj[1, 1] = focal
    proj[2, 2] = -(far + near) / (far - near)
    proj[2, 3] = -2 * far * near / (far - near)
    proj[3, 2] = -1
    proj[0, 2], proj[1, 2] = jitter[0] / w, jitter[1] / h
    return dict(view=view, proj=proj, eye=np.asarray(eye, dtype=float), w=w, h=h, forward=forward)


def plane(point, normal, ident, rect=None):
    """A plane through `point`. rect=(u_axis, v_axis, half_u, half_v) bounds it to a rectangle."""
    return dict(point=point, normal=normal, id=ident, rect=rect)


def cast(np, cam, surfaces):
    """Nearest hit per pixel centre -> dict(depth, normals, world, ident, t)."""
    w, h = cam['w'], cam['h']
    x, y = np.meshgrid((np.arange(w) + .5) / w * 2 - 1, (np.arange(h) + .5) / h * 2 - 1)
    ndc = np.stack([x, y, np.zeros_like(x), np.ones_like(x)], axis=-1)
    world = ndc @ np.linalg.inv(cam['proj'] @ cam['view']).T
    direction = world[:, :, :3] / world[:, :, 3:] - cam['eye']
    direction /= np.linalg.norm(direction, axis=-1, keepdims=True)
    best_t = np.full((h, w), np.inf)
    ident = np.full((h, w), -1, int)
    normal = np.zeros((h, w, 3))
    for s in surfaces:
        n = np.asarray(s['normal'], dtype=float)
        n = n / np.linalg.norm(n)
        denominator = direction @ n
        with np.errstate(divide='ignore', invalid='ignore'):
            t = ((np.asarray(s['point']) - cam['eye']) @ n) / denominator
        hit = cam['eye'] + direction * t[:, :, None]
        ok = np.isfinite(t) & (t > 0)
        if s['rect']:
            u, v, hu, hv = s['rect']
            rel = hit - np.asarray(s['point'])
            ok &= (np.abs(rel @ np.asarray(u)) <= hu) & (np.abs(rel @ np.asarray(v)) <= hv)
        ok &= t < best_t
        best_t = np.where(ok, t, best_t)
        ident = np.where(ok, s['id'], ident)
        facing = np.where((denominator < 0)[:, :, None], n, -n)
        normal = np.where(ok[:, :, None], facing, normal)
    hit = cam['eye'] + direction * np.where(np.isfinite(best_t), best_t, 0)[:, :, None]
    clip = np.concatenate([hit, np.ones((h, w, 1))], axis=2) @ (cam['proj'] @ cam['view']).T
    depth = np.where(np.isfinite(best_t), (clip[:, :, 2] / clip[:, :, 3] + 1) / 2, 1.0)
    view_normal = normal @ cam['view'][:3, :3].T
    encoded = view_normal / np.maximum(np.abs(view_normal).sum(axis=-1, keepdims=True), 1e-9)
    normals = np.zeros((h, w, 4), np.float32)
    normals[:, :, :2] = encoded[:, :, :2] * .5 + .5
    depth4 = np.repeat(depth[:, :, None], 4, axis=2).astype(np.float32)
    return dict(depth=depth4, normals=normals, projection=cam['proj'], inverse=np.linalg.inv(cam['proj']),
                world=hit, ident=ident, normal_world=normal, t=best_t, camera=cam)


def receiver_bias(np, scene, trace_w, trace_h):
    """areaReceiverBias for every pixel: 1.5 footprints of one TRACE pixel at the pixel's own depth, >= 2 cm."""
    cam = scene['camera']
    view_z = -(np.concatenate([scene['world'], np.ones(scene['world'].shape[:2] + (1,))], axis=2)
               @ cam['view'].T)[:, :, 2]
    fx = view_z * 2 / (cam['proj'][0, 0] * trace_w)
    fy = view_z * 2 / (cam['proj'][1, 1] * trace_h)
    return np.maximum(.02, 1.5 * np.maximum(fx, fy))


def visible_fraction(np, origins, light, radius, blockers, disk=(12, 24)):
    """Fraction of the disk light (centre `light`, perpendicular to the origin->centre direction) that
    reaches each origin without crossing a blocker rectangle. origins: (n, 3)."""
    radius = radius if radius > 0 else .1
    nr, na = disk
    r = np.sqrt((np.arange(nr) + .5) / nr) * radius
    a = (np.arange(na) + .5) / na * 2 * np.pi
    rr, aa = np.meshgrid(r, a, indexing='ij')
    rr, aa = rr.ravel(), aa.ravel()
    light = np.asarray(light, dtype=float)
    out = np.empty(len(origins))
    for lo in range(0, len(origins), 512):
        o = origins[lo:lo + 512]
        direction = light - o
        direction /= np.linalg.norm(direction, axis=-1, keepdims=True)
        up = np.where(np.abs(direction[:, 2:3]) < .99, np.array([0, 0, 1.]), np.array([0, 1., 0]))
        tangent = np.cross(direction, up)
        tangent /= np.linalg.norm(tangent, axis=-1, keepdims=True)
        bitangent = np.cross(direction, tangent)
        target = light[None, None, :] + rr[None, :, None] * (
            np.cos(aa)[None, :, None] * tangent[:, None, :] + np.sin(aa)[None, :, None] * bitangent[:, None, :])
        delta = target - o[:, None, :]
        blocked = np.zeros(delta.shape[:2], bool)
        for s in blockers:
            n = np.asarray(s['normal'], dtype=float)
            n /= np.linalg.norm(n)
            denominator = delta @ n
            with np.errstate(divide='ignore', invalid='ignore'):
                t = ((np.asarray(s['point']) - o) @ n)[:, None] / denominator
            ok = np.isfinite(t) & (t > 0) & (t < 1)
            hit = o[:, None, :] + delta * t[:, :, None]
            if s['rect']:
                u, v, hu, hv = s['rect']
                rel = hit - np.asarray(s['point'])
                ok &= (np.abs(rel @ np.asarray(u)) <= hu) & (np.abs(rel @ np.asarray(v)) <= hv)
            blocked |= ok
        out[lo:lo + 512] = 1 - blocked.mean(axis=1)
    return out


def hit_pixel_oracle(np, scene, light, blockers_by_id, bias, thickness=None):
    """Point-light geometry oracle against THIS frame's texels, as in the moving-camera gate: a receiver
    pixel is dark when the ray to the light crosses a blocker surface at a point whose pixel is that
    blocker in the G-buffer. Returns uint8 (255 lit / 0 dark) for every pixel (blocker pixels: 255)."""
    cam = scene['camera']
    h, w = scene['ident'].shape
    origin = scene['world'] + scene['normal_world'] * bias[:, :, None]
    light = np.asarray(light, dtype=float)
    dark = np.zeros((h, w), bool)
    vp = cam['proj'] @ cam['view']
    for ident, s in blockers_by_id.items():
        n = np.asarray(s['normal'], dtype=float)
        n /= np.linalg.norm(n)
        delta = light - origin
        denominator = delta @ n
        with np.errstate(divide='ignore', invalid='ignore'):
            t = ((np.asarray(s['point']) - origin) @ n) / denominator
        ok = np.isfinite(t) & (t > 0) & (t < 1)
        hit = origin + delta * t[:, :, None]
        clip = np.concatenate([hit, np.ones((h, w, 1))], axis=2) @ vp.T
        with np.errstate(divide='ignore', invalid='ignore'):
            uv = clip[:, :, :2] / clip[:, :, 3:] * .5 + .5
        pixel = np.floor(uv * np.array([w, h]) + 1e-3).astype(int)
        on_screen = ok & np.all((pixel >= 0) & (pixel < np.array([w, h])), axis=2) & (clip[:, :, 3] > 0)
        pixel = np.clip(pixel, 0, np.array([w - 1, h - 1]))
        on_blocker = scene['ident'][pixel[:, :, 1], pixel[:, :, 0]] == ident
        dark |= on_screen & on_blocker & (scene['ident'] != ident)
    image = np.full((h, w), 255, np.uint8)
    image[dark] = 0
    return image
