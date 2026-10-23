"""Worker-process tasks (CPU): G-buffer, ground truth, probes. Importable by spawn-started pool workers."""
import hashlib
import json
import os
from functools import lru_cache
from pathlib import Path

import numpy as np

import relight_3d_scene as S
import relight_3d_metrics as M

MIN_BLOCKER_PX = 40      # a key blocker group counts as ON-SCREEN with at least this many 1080p pixels
PROBE_Z_TOL = (0.02, 0.004)


@lru_cache(maxsize=None)
def get_scene(name):
    return S.SCENES[name]()


def _key(*parts):
    return hashlib.sha1(json.dumps(parts, sort_keys=True, default=str).encode()).hexdigest()[:16]


def blocker_info(sc, cam, g):
    counts = np.bincount(g['surf'].ravel(), minlength=256)
    info = {}
    for grp, names in sc.groups.items():
        ids = [sc.prim_index(n) for n in names]
        px = int(sum(counts[i] for i in ids))
        lo = np.min([sc.prims[i].aabb()[0] for i in ids], 0)
        hi = np.max([sc.prims[i].aabb()[1] for i in ids], 0)
        corners = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
        px_, py_, zf = cam.project(corners)
        in_frustum = bool(np.any((zf > cam.near) & (px_ > 0) & (px_ < cam.W) & (py_ > 0) & (py_ < cam.H)) or
                          (np.all(zf > cam.near) and px_.min() < 0 and px_.max() > cam.W))
        status = 'on' if px >= MIN_BLOCKER_PX else ('hidden' if in_frustum else 'off')
        info[grp] = {'px': px, 'status': status}
    return info


def probe_projection(sc, cam, g):
    """Pixel position of every floor probe point and whether it is directly visible."""
    pts = sc.probes
    px, py, zf = cam.project(pts)
    ok = (zf > cam.near) & (px >= 0.5) & (px <= cam.W - 0.5) & (py >= 0.5) & (py <= cam.H - 0.5)
    ii = np.clip(px.astype(np.int64), 0, cam.W - 1)
    jj = np.clip(py.astype(np.int64), 0, cam.H - 1)
    zb = g['zf'][jj, ii].astype(np.float64)
    ok &= np.abs(zb - zf) < PROBE_Z_TOL[0] + PROBE_Z_TOL[1] * zf
    xy = np.stack([px, py], 1).astype(np.float32)
    xy[~ok] = np.nan
    return xy


def compute_gt(sc, pose, cam, g, stride, samples):
    H, W = cam.H, cam.W
    Hg, Wg = H // stride, W // stride
    gj, gi = np.mgrid[0:Hg, 0:Wg]
    fj, fi = gj * stride + stride // 2, gi * stride + stride // 2
    zl = g['zf'][fj, fi]
    surf = g['surf'][fj, fi]
    pos = g['pos'][fj, fi]
    valid = np.isfinite(zl).ravel()
    P = pos.reshape(-1, 3).astype(np.float64)
    N = g['nrm'][fj, fi].reshape(-1, 3).astype(np.float64)
    lights = []
    for light in sc.light_sets[pose.cfg]:
        L0, cand = S.unshadowed(light, P, N)
        cand &= valid
        idx = np.nonzero(cand)[0]
        idx = idx[S.tile_order(idx % Wg, idx // Wg)]
        res = S.trace_light(sc, light, P[idx], N[idx], idx.astype(np.uint32) + 977 * pose.cfg, S=samples, cam=cam, zbuf=g['zf'])
        nblk = np.zeros(Hg * Wg, np.uint16)
        reasons = np.zeros((Hg * Wg, 4), np.uint16)
        tmean = np.full(Hg * Wg, S.INF, np.float32)
        nblk[idx] = res['nblk']
        reasons[idx] = res['reasons']
        tmean[idx] = res['tmean']
        lights.append({'nblk': nblk.reshape(Hg, Wg), 'reasons': reasons.reshape(Hg, Wg, 4), 'tmean': tmean.reshape(Hg, Wg),
                       'cand': cand.reshape(Hg, Wg), 'L0': L0.reshape(Hg, Wg).astype(np.float16)})
    return {'surf': surf, 'zl': zl.astype(np.float32), 'pos': pos.astype(np.float32), 'lights': lights}


def save_gt(path, gt):
    flat = {'surf': gt['surf'], 'zl': gt['zl'], 'pos': gt['pos'], 'n': np.array(len(gt['lights']))}
    for i, L in enumerate(gt['lights']):
        for k, v in L.items():
            flat['l%d_%s' % (i, k)] = v
    np.savez_compressed(path, **flat)


def load_gt(path, samples):
    z = np.load(path)
    gt = {'surf': z['surf'], 'zl': z['zl'], 'pos': z['pos'], 'lights': []}
    for i in range(int(z['n'])):
        L = {k: z['l%d_%s' % (i, k)] for k in ('nblk', 'reasons', 'tmean', 'cand', 'L0')}
        L['L0'] = L['L0'].astype(np.float32)
        L['vis'] = (1.0 - L['nblk'].astype(np.float32) / samples).astype(np.float32)
        L['cls'] = M.classify(L['nblk'].astype(np.int32), L['reasons'])
        gt['lights'].append(L)
    return gt


def probe_cls(sc, pose, cam, g, probe_xy, samples):
    """Blocker category of every directly visible floor probe in THIS pose: 0 on-screen / 1-3 failure / 255 unshadowed."""
    vis = np.isfinite(probe_xy[:, 0])
    out = np.full((len(sc.light_sets[pose.cfg]), len(probe_xy)), 255, np.uint8)
    idx = np.nonzero(vis)[0]
    if not len(idx):
        return out
    P = sc.probes[idx].astype(np.float64)
    N = np.tile(np.array(sc.probe_normal, np.float64), (len(P), 1))
    for li, light in enumerate(sc.light_sets[pose.cfg]):
        r = S.trace_light(sc, light, P, N, np.arange(len(P), dtype=np.uint32), S=samples, cam=cam, zbuf=g['zf'])
        out[li, idx] = M.classify(r['nblk'].astype(np.int32), r['reasons'])
    return out


def pose_task(args):
    """Rasterise the exact G-buffer for one pose, write the GPU inputs, and return (cached) ground truth."""
    scene_name, pidx, opt = args
    sc = get_scene(scene_name)
    pose = sc.poses[pidx]
    W, H, stride, samples = opt['W'], opt['H'], opt['stride'], opt['samples']
    cam = S.Camera(pose.eye, pose.target, W, H, pose.vfov)
    g = S.cast_primary(sc, cam)
    depth24, depth32, norm16 = S.encode_gbuffer(cam, g)
    work = Path(opt['work'])
    work.mkdir(parents=True, exist_ok=True)
    gb = work / (pose.id + '.gb.npz')
    np.savez(gb, depth32=depth32, norm16=norm16, zf=g['zf'], surf=g['surf'])
    key = _key(sc.signature(), pose.spec(), W, H, stride, samples)
    cache = Path(opt['cache']) / ('gt_%s_%s.npz' % (pose.id, key))
    if not cache.exists():
        gt = compute_gt(sc, pose, cam, g, stride, samples)
        cache.parent.mkdir(parents=True, exist_ok=True)
        tmp = cache.with_name(cache.name + '.%d.tmp.npz' % os.getpid())
        save_gt(tmp, gt)
        os.replace(tmp, cache)
    gt = load_gt(cache, samples)
    probe_xy = probe_projection(sc, cam, g)
    return {'scene': scene_name, 'pidx': pidx, 'id': pose.id, 'gb': str(gb), 'gt': gt,
            'probe_xy': probe_xy, 'probe_cls': probe_cls(sc, pose, cam, g, probe_xy, samples),
            'blockers': blocker_info(sc, cam, g)}


def probe_gt_task(args):
    """Camera-independent ground truth of every floor probe for one light configuration."""
    scene_name, cfg, opt = args
    sc = get_scene(scene_name)
    samples = opt['samples']
    key = _key(sc.signature(), 'probe', cfg, samples)
    cache = Path(opt['cache']) / ('probe_%s_%d_%s.npy' % (scene_name, cfg, key))
    if cache.exists():
        return scene_name, cfg, np.load(cache)
    P = sc.probes.astype(np.float64)
    N = np.tile(np.array(sc.probe_normal, np.float64), (len(P), 1))
    out = []
    for light in sc.light_sets[cfg]:
        res = S.trace_light(sc, light, P, N, np.arange(len(P), dtype=np.uint32) + 977 * cfg, S=samples)
        out.append(res['vis'])
    arr = np.stack(out).astype(np.float32)
    cache.parent.mkdir(parents=True, exist_ok=True)
    tmp = cache.with_name(cache.name + '.%d.tmp.npy' % os.getpid())
    np.save(tmp, arr)
    os.replace(tmp, cache)
    return scene_name, cfg, arr
