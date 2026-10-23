"""3D ground-truth validation harness for Relight soft shadows.

Runs the PRODUCTION screen-space area-shadow gather + bilateral filter/upsample shaders (hidden WGL, shader source
read from a git ref, never the working tree) on exact analytic-scene G-buffers, and compares the result with a CPU
ray-traced ground truth of the true area-light visibility.

Scenes (scripts/code_tools/relight_3d_scene.py):
  R  room corner, boxes (one floating), sphere, actor capsule, 4 cm railing, point + projector light (24 orbit poses
     at 2 heights x 2 distances + 3 expected-failure poses: blocker off-screen / hidden)
  A  balcony railing (user acceptance scene A): full 360 deg orbit, 3 heights x 2 distances
  B  0.5 m sphere on a floor, light flying a circle while the camera orbits (user acceptance scene B)
  C  analytic replica of the user's night balcony (user acceptance scene C): housed wall lamp, crates, pipe, railing

Pluggable visibility source (for the P1-lite cube-map spike): --source screen|oracle|lit|path/to/file.py:Class.
A source implements `configs(ctx)` and `visibility(frame, cfg)` (full-resolution (L,H,W) float32 in [0,1]; the
harness samples the ground-truth grid and probe points from it), or `sampled(frame, cfg, grid, probes)` for a
source that works at points. `frame` carries the scene, pose, camera, lights, G-buffer and exact world positions.

  py -3 -I scripts/code_tools/relight_3d_reference.py                  # everything, HEAD shaders
  py -3 -I scripts/code_tools/relight_3d_reference.py --scenes A,C --shader-ref <commit>|WORKTREE
  py -3 -I scripts/code_tools/relight_3d_reference.py --controls       # oracle/lit negative+positive controls

Outputs go to <out> (default I:/alchemy-machinima/.tmp/ref3d): sheets/, results/, summary.txt, results.json.
"""
import argparse
import importlib.util
import json
import math
import multiprocessing as mp
import os
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import numpy as np  # noqa: E402

np.seterr(over='ignore', invalid='ignore', divide='ignore')
import relight_3d_scene as S  # noqa: E402
import relight_3d_metrics as M  # noqa: E402
import relight_3d_tasks as T  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
OUT_DEFAULT = REPO / '.tmp' / 'ref3d'
SPAWN = mp.get_context('spawn')

# --------------------------------------------------------------------------------------------- gates
# Unexpected-category gates (blocker ON-SCREEN, or no blocker at all). Values are set from the HEAD 4dd9ff661c1
# baseline run, see GATE_NOTES; the expected category (off-screen / hidden / absent blockers) is reported, not gated.
# metric: threshold applied to the WORST POSE of the scene and config. Keys: scene -> config -> metric.
GATES = {
    'A': {
        'Default': {'mae_on': 0.3264, 'edge_mean_on': 63.1076, 'edge80_mean_on': 47.8587, 'edge_miss': 1.0, 'edge80_miss': 0.9472, 'leak_on': 1.0, 'false_dark': 0.0534, 'flicker_pop': 0.2675, 'pen_cover_near': 0.5624, 'pen_cover_far': 0.0997},
        'Default+Thin': {'mae_on': 0.2664, 'edge_mean_on': 36.1632, 'edge80_mean_on': 30.8915, 'edge_miss': 0.7781, 'edge80_miss': 0.6732, 'leak_on': 0.7483, 'false_dark': 0.1532, 'flicker_pop': 0.2781, 'pen_cover_near': 0.5337, 'pen_cover_far': 0.3383},
        'Cinematic': {'mae_on': 0.2897, 'edge_mean_on': 53.3516, 'edge80_mean_on': 35.7641, 'edge_miss': 0.9655, 'edge80_miss': 0.743, 'leak_on': 0.9936, 'false_dark': 0.0997, 'flicker_pop': 0.3075, 'pen_cover_near': 0.6562, 'pen_cover_far': 0.2309},
        'Cinematic+Thin': {'mae_on': 0.2668, 'edge_mean_on': 35.952, 'edge80_mean_on': 30.396, 'edge_miss': 0.7887, 'edge80_miss': 0.6777, 'leak_on': 0.7571, 'false_dark': 0.154, 'flicker_pop': 0.2781, 'pen_cover_near': 0.5377, 'pen_cover_far': 0.3389},
    },
    'B': {
        'Default': {'mae_on': 0.588, 'edge_mean_on': 24.5394, 'edge80_mean_on': 33.6659, 'edge_miss': 0.6721, 'edge80_miss': 0.792, 'leak_on': 0.4275, 'false_dark': 0.0098, 'flicker_pop': 0.4185, 'pen_cover_near': 0.6593, 'pen_cover_far': 0.3669},
        'Default+Thin': {'mae_on': 0.1642, 'edge_mean_on': 12.6338, 'edge80_mean_on': 15.4065, 'edge_miss': 0.3761, 'edge80_miss': 0.4699, 'leak_on': 0.1091, 'false_dark': 0.0111, 'flicker_pop': 0.266, 'pen_cover_near': 0.6768, 'pen_cover_far': 0.7275},
        'Cinematic': {'mae_on': 0.2849, 'edge_mean_on': 16.8774, 'edge80_mean_on': 20.4704, 'edge_miss': 0.544, 'edge80_miss': 0.6417, 'leak_on': 0.1763, 'false_dark': 0.0109, 'flicker_pop': 0.282, 'pen_cover_near': 0.6755, 'pen_cover_far': 0.6497},
        'Cinematic+Thin': {'mae_on': 0.1648, 'edge_mean_on': 11.8498, 'edge80_mean_on': 15.496, 'edge_miss': 0.3695, 'edge80_miss': 0.4729, 'leak_on': 0.1108, 'false_dark': 0.0111, 'flicker_pop': 0.2588, 'pen_cover_near': 0.6729, 'pen_cover_far': 0.7227},
    },
    'C': {
        'Default': {'mae_on': 0.3401, 'edge_mean_on': 50.7544, 'edge80_mean_on': 45.4227, 'edge_miss': 0.8627, 'edge80_miss': 0.8053, 'leak_on': 0.5275, 'false_dark': 0.0091, 'flicker_pop': 0.0937, 'pool_dark': 0.0052, 'pen_cover_near': 0.5586, 'pen_cover_far': 0.2839},
        'Default+Thin': {'mae_on': 0.2516, 'edge_mean_on': 41.0288, 'edge80_mean_on': 42.1398, 'edge_miss': 0.7645, 'edge80_miss': 0.7599, 'leak_on': 0.3004, 'false_dark': 0.028, 'flicker_pop': 0.1538, 'pool_dark': 0.0053, 'pen_cover_near': 0.7326, 'pen_cover_far': 0.4473},
        'Cinematic': {'mae_on': 0.2901, 'edge_mean_on': 45.602, 'edge80_mean_on': 43.4464, 'edge_miss': 0.8188, 'edge80_miss': 0.766, 'leak_on': 0.4134, 'false_dark': 0.0149, 'flicker_pop': 0.0494, 'pool_dark': 0.0052, 'pen_cover_near': 0.681, 'pen_cover_far': 0.3677},
        'Cinematic+Thin': {'mae_on': 0.2522, 'edge_mean_on': 43.3925, 'edge80_mean_on': 43.1477, 'edge_miss': 0.7952, 'edge80_miss': 0.7614, 'leak_on': 0.3012, 'false_dark': 0.0279, 'flicker_pop': 0.1454, 'pool_dark': 0.0053, 'pen_cover_near': 0.7323, 'pen_cover_far': 0.4445},
    },
    'R': {
        'Default': {'mae_on': 0.5604, 'edge_mean_on': 36.653, 'edge80_mean_on': 30.2874, 'edge_miss': 0.7421, 'edge80_miss': 0.6075, 'leak_on': 0.4547, 'false_dark': 0.0495, 'flicker_pop': 0.4084, 'pen_cover_near': 0.4125, 'pen_cover_far': 0.3283},
        'Default+Thin': {'mae_on': 0.2184, 'edge_mean_on': 19.404, 'edge80_mean_on': 19.15, 'edge_miss': 0.4764, 'edge80_miss': 0.4657, 'leak_on': 0.1969, 'false_dark': 0.0318, 'flicker_pop': 0.2672, 'pen_cover_near': 0.6338, 'pen_cover_far': 0.6362},
        'Cinematic': {'mae_on': 0.2946, 'edge_mean_on': 30.8993, 'edge80_mean_on': 22.435, 'edge_miss': 0.6338, 'edge80_miss': 0.4867, 'leak_on': 0.2555, 'false_dark': 0.0381, 'flicker_pop': 0.3344, 'pen_cover_near': 0.5321, 'pen_cover_far': 0.5202},
        'Cinematic+Thin': {'mae_on': 0.2185, 'edge_mean_on': 19.3542, 'edge80_mean_on': 19.2143, 'edge_miss': 0.4764, 'edge80_miss': 0.4709, 'leak_on': 0.1973, 'false_dark': 0.0318, 'flicker_pop': 0.2783, 'pen_cover_near': 0.6353, 'pen_cover_far': 0.6367},
    },
}
GATE_NOTES = (
    "Regression ratchet, NOT acceptance: every limit is the HEAD 4dd9ff661c1 baseline value of the same scene/config/metric "
    "x1.10 (+0.005 absolute; fractions capped at 1, edge means at the 64 px cap; minimum-type metrics x0.90). The harness is "
    "deterministic (fixed 256-ray ground-truth pattern with a pixel-hash rotation, hash-seeded production noise cells, byte-stable GPU "
    "readbacks), so run-to-run variance is zero and the 10% only absorbs driver or compiler differences. Limits are the worst on-screen "
    "pose of each scene; a change that makes any of them worse by more than 10% fails. They are intentionally not tuned to a pass "
    "criterion: the absolute ACCEPT_* table shows that today's production fails the user's criteria, and --strict exits non-zero on that."
)


# --------------------------------------------------------------------------------------------- sources
class Frame:
    """Everything a visibility source may need for one pose."""

    def __init__(self, scene, pose, cam, lights, depth32, norm16, zf, surf, gt, probe_xy, probe_gt):
        self.scene, self.pose, self.cam, self.lights = scene, pose, cam, lights
        self.depth32, self.norm16, self.zf, self.surf = depth32, norm16, zf, surf
        self.gt, self.probe_xy, self.probe_gt = gt, probe_xy, probe_gt

    def world_pos(self):
        """Exact world position of every pixel centre, (H,W,3) float32 (NaN for sky)."""
        c = self.cam
        d = c.rays(0, c.H)
        p = c.eye + d * (self.zf / (d @ c.f))[..., None]
        return p.astype(np.float32)


class VisibilitySource:
    name = 'base'

    def configs(self, ctx):
        """-> list of (config name, params)"""
        return [('default', {})]

    def prepare(self, ctx):
        pass

    def visibility(self, frame, cfg):
        raise NotImplementedError

    def sampled(self, frame, cfg, grid, probes):
        """grid: (fj, fi) index arrays; probes: (K,2) pixel coords with NaN for invisible.
        Returns ((L,Hg,Wg) visibility, (L,K) visibility at probes)."""
        v = self.visibility(frame, cfg)
        return sample_full(v, grid, probes)


def sample_full(v, grid, probes):
    fj, fi = grid
    g = v[:, fj, fi]
    L, H, W = v.shape
    ok = np.isfinite(probes[:, 0])
    x = np.where(ok, probes[:, 0], 0.5) - 0.5
    y = np.where(ok, probes[:, 1], 0.5) - 0.5
    x0 = np.clip(np.floor(x).astype(np.int64), 0, W - 2)
    y0 = np.clip(np.floor(y).astype(np.int64), 0, H - 2)
    fx = np.clip(x - x0, 0, 1)
    fy = np.clip(y - y0, 0, 1)
    p = ((v[:, y0, x0] * (1 - fx) + v[:, y0, x0 + 1] * fx) * (1 - fy) +
         (v[:, y0 + 1, x0] * (1 - fx) + v[:, y0 + 1, x0 + 1] * fx) * fy)
    p[:, ~ok] = np.nan
    return g.astype(np.float32), p.astype(np.float32)


class ScreenSpaceProduction(VisibilitySource):
    """Today's production: areaShadowF gather -> separable blur -> areaShadowFilterF upsample (GPU, from --shader-ref)."""
    name = 'screen'

    def prepare(self, ctx):
        import relight_3d_gl as G
        import relight_3d_plan as PL
        self.tree = G.SourceTree(ctx['args'].shader_ref)
        self.plans, self.plan_source = PL.compute_plans(self.tree, ctx['out'])
        self.runner = G.AreaRunner(self.tree)
        ctx['renderer'] = self.runner.gl.renderer
        ctx['shader_commit'] = self.tree.commit()
        ctx['plan_source'] = self.plan_source

    def configs(self, ctx):
        import relight_3d_plan as PL
        return [(name, {}) for name, _, _ in PL.CONFIGS]

    def visibility(self, frame, cfg):
        plan = self.plans[cfg[0]][len(frame.lights)]
        return self.runner.run(frame.cam, frame.depth32, frame.norm16, frame.lights, plan)

    def plan_text(self, name):
        p = self.plans[name][2]
        return '%d rays x %d steps%s %s' % (p['samples'], p['steps'], ' (48 near)' if p['near_steps'] else '',
                                            'half-res' if p['half'] else 'full-res')


class OracleSource(VisibilitySource):
    """Positive control: returns the ground truth itself (every error metric must be ~0, every gate must pass)."""
    name = 'oracle'

    def configs(self, ctx):
        return [('oracle', {})]

    def sampled(self, frame, cfg, grid, probes):
        g = np.stack([np.where(L['cand'], L['vis'], 1.0) for L in frame.gt['lights']]).astype(np.float32)
        p = np.where(np.isfinite(probes[:, 0])[None, :], frame.probe_gt, np.nan).astype(np.float32)
        return g, p


class LitSource(VisibilitySource):
    """Negative control: no shadows at all (every on-screen shadow must be reported as a leak and fail the gates)."""
    name = 'lit'

    def configs(self, ctx):
        return [('lit', {})]

    def sampled(self, frame, cfg, grid, probes):
        L = len(frame.lights)
        g = np.ones((L,) + frame.gt['surf'].shape, np.float32)
        p = np.where(np.isfinite(probes[:, 0])[None, :], np.ones((L, len(probes)), np.float32), np.nan)
        return g, p


def load_source(spec):
    if spec == 'screen':
        return ScreenSpaceProduction()
    if spec == 'oracle':
        return OracleSource()
    if spec == 'lit':
        return LitSource()
    path, _, cls = spec.partition(':')
    mod_spec = importlib.util.spec_from_file_location('relight_3d_user_source', path)
    mod = importlib.util.module_from_spec(mod_spec)
    mod_spec.loader.exec_module(mod)
    return getattr(mod, cls or 'Source')()


# --------------------------------------------------------------------------------------------- metrics task
def pool_spec(sc):
    if not sc.pool:
        return None
    ids = [sc.prim_index(n) for n in sc.pool['surf']]
    light = sc.light_sets[0][sc.pool['light']]
    return {'light': sc.pool['light'], 'ids': ids, 'radius': sc.pool['radius'], 'pos': list(light.pos)}


def metrics_task(args):
    """Metrics for every config of one pose + its contact sheet (runs in a worker)."""
    import relight_3d_sheets as SH
    scene_name, pidx, gt, prods, plan_text, sheet_path, stride, summary, make_sheet = args
    sc = T.get_scene(scene_name)
    pose = sc.poses[pidx]
    pool = pool_spec(sc)
    res, rows = {}, []
    for name, prod in prods.items():
        m = M.eval_pose(gt, prod, stride, pool)
        res[name] = m
        text = 'mae_on %.3f  edge_p95 %.1fpx  leak %.0f%%  acne %.1f%%  | expected: mae %.3f leak %.0f%%' % (
            m['mae_on'], m['edge_p95_on'], 100 * (m['leak_on'] if m['leak_on'] == m['leak_on'] else 0),
            100 * (m['false_dark'] if m['false_dark'] == m['false_dark'] else 0), m['mae_exp'],
            100 * (m['leak_exp'] if m['leak_exp'] == m['leak_exp'] else 0)) + (
            '   %s' % plan_text.get(name, ''))
        rows.append((name, prod, text))
    if make_sheet:
        SH.contact_sheet(sheet_path, '%s   [%s]   %s' % (pose.id, pose.tag, pose.note), summary, gt, rows,
                         [p.albedo for p in sc.prims])
    thumbs = {}
    albedo = [p.albedo for p in sc.prims]
    names = list(prods)
    for name in sorted({names[0], names[-1]}):
        gt_vis = [L['vis'] * L['cand'] + (~L['cand']) for L in gt['lights']]
        thumbs[name] = (SH.thumb(SH.shade(gt, gt_vis, albedo)), SH.thumb(SH.shade(gt, prods[name], albedo)))
    return scene_name, pidx, res, thumbs


# --------------------------------------------------------------------------------------------- main
def fmt(x, nd=3):
    return '  n/a ' if x is None or (isinstance(x, float) and x != x) else ('%.*f' % (nd, x))


def selftest(out):
    """Cheap harness checks: tracer analytic cases, G-buffer encode round trip, determinism."""
    sc = S.Scene('T', [S.Prim('floor', 'obb', c=(0, 0, -0.15), h=(10, 10, 0.15)),
                       S.Prim('slab', 'obb', c=(0, 0, 2.0), h=(5, 5, 0.05))], [], [], {}, np.zeros((0, 3)))
    light = S.Light('l', (0, 0, 4.0), 0.5, 20.0)
    P = np.array([[0, 0, 0], [14.0, 0, 0], [0, 4.99, 0]], np.float64)
    N = np.tile([0, 0, 1.0], (3, 1))
    r = S.trace_light(sc, light, P, N, np.arange(3, dtype=np.uint32), S=256)
    assert r['vis'][0] == 0.0, ('umbra under slab must be fully shadowed', r['vis'])
    assert r['vis'][1] == 1.0, ('far point must be lit', r['vis'])
    # half-occluded disk: slab edge at x=3 (height 2), light disk radius .5 at height 4, receiver on the floor at x=
    # such that the edge cuts the disk through its centre: line from edge (3,2) through disk centre (0,4)? use
    # analytic: point where the segment to the disk centre just grazes the edge: (0,4)->(3,2) extended to z=0 => x=6
    sc2 = S.Scene('T2', [S.Prim('floor', 'obb', c=(0, 0, -0.15), h=(10, 10, 0.15)),
                         S.Prim('half', 'obb', c=(-47.0, 0, 2.0), h=(50, 50, 0.0005))], [], [], {}, np.zeros((0, 3)))
    light2 = S.Light('l', (0, 0, 4.0), 0.5, 20.0)
    r2 = S.trace_light(sc2, light2, np.array([[6.0, 0, 0]]), np.array([[0, 0, 1.0]]), np.arange(1, dtype=np.uint32), S=1024)
    assert abs(r2['vis'][0] - 0.5) < 0.03, ('half-occluded disk', r2['vis'])
    r2b = S.trace_light(sc2, light2, np.array([[6.0, 0, 0]]), np.array([[0, 0, 1.0]]), np.arange(1, dtype=np.uint32), S=1024)
    assert r2b['vis'].tobytes() == r2['vis'].tobytes(), 'tracer determinism'
    cam = S.Camera((2, -6, 3), (0, 0, 1), 64, 36)
    sc3 = S.scene_sphere()
    g = S.cast_primary(sc3, cam)
    d24, d32, n16 = S.encode_gbuffer(cam, g)
    m = np.isfinite(g['zf'])
    nv = g['nrm'][m].astype(np.float64) @ cam.view[:3, :3].T
    dec = S.oct_decode(n16[m][:, :2] / 65535.0)
    assert np.degrees(np.arccos(np.clip((nv * dec).sum(1), -1, 1))).max() < 0.05, 'normal encode round trip'
    ndc = d24[m].astype(np.float64) / 16777215 * 2 - 1
    zr = 2 * S.FAR * S.NEAR / ((S.FAR + S.NEAR) - ndc * (S.FAR - S.NEAR))
    assert np.abs(zr - g['zf'][m]).max() < 0.01, 'depth round trip'
    print('[ref3d] self-test PASS: umbra=0, lit=1, half-occluded disk=%.3f, deterministic, normals/depth round trip' % r2['vis'][0],
          flush=True)


def build_tasks(args):
    wanted = [s for s in args.scenes.split(',') if s]
    tasks = []
    for name in wanted:
        sc = T.get_scene(name)
        for pidx, pose in enumerate(sc.poses):
            if args.pose_filter and args.pose_filter not in pose.id:
                continue
            tasks.append((name, pidx))
    if args.limit:
        per = {}
        keep = []
        for name, pidx in tasks:
            per[name] = per.get(name, 0) + 1
            if per[name] <= args.limit:
                keep.append((name, pidx))
        tasks = keep
    return tasks


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenes', default='R,A,B,C')
    ap.add_argument('--shader-ref', default='HEAD', help='git ref the production shaders/planner are read from, or WORKTREE')
    ap.add_argument('--source', default='screen', help='screen | oracle | lit | path.py:Class')
    ap.add_argument('--out', default=str(OUT_DEFAULT))
    ap.add_argument('--cache', default=str(OUT_DEFAULT / 'cache'), help='ground-truth cache (content keyed; shader independent)')
    ap.add_argument('--res', default='1920x1080', help='render resolution; production planning assumes 1080p')
    ap.add_argument('--stride', type=int, default=4, help='ground-truth grid stride in pixels')
    ap.add_argument('--gt-samples', type=int, default=256)
    ap.add_argument('--jobs', type=int, default=24)
    ap.add_argument('--limit', type=int, default=0, help='debug: first N poses per scene')
    ap.add_argument('--pose-filter', default='')
    ap.add_argument('--no-sheets', action='store_true')
    ap.add_argument('--controls', action='store_true', help='run the oracle (must pass) and lit (must fail) controls on a subset')
    ap.add_argument('--keep-gbuffers', action='store_true')
    ap.add_argument('--no-gates', action='store_true')
    ap.add_argument('--print-gates', action='store_true', help='write gates_proposed.json (baseline * margin) for this run')
    ap.add_argument('--strict', action='store_true', help='exit non-zero on acceptance failures too')
    args = ap.parse_args()
    out = Path(args.out)
    if args.controls:
        return run_controls(args)
    return run(args, out)


def run_controls(args):
    selftest(args.out)
    import copy
    results = {}
    for src in ('oracle', 'lit'):
        a = copy.copy(args)
        a.source = src
        a.scenes = 'R,A'
        a.limit = 3
        a.force_gates = True
        a.no_sheets = True
        a.controls = False
        results[src] = run(a, Path(args.out) / ('controls_' + src), quiet=True)
    ok = results['oracle']['gate_failures'] == 0 and results['lit']['gate_failures'] > 0
    print('[ref3d] CONTROLS %s: oracle gate failures=%d (must be 0), lit gate failures=%d (must be >0)' %
          ('PASS' if ok else 'FAIL', results['oracle']['gate_failures'], results['lit']['gate_failures']), flush=True)
    return 0 if ok else 1


def run(args, out, quiet=False):
    t_start = time.time()
    out.mkdir(parents=True, exist_ok=True)
    W, H = map(int, args.res.lower().split('x'))
    stride = args.stride
    Hg, Wg = H // stride, W // stride
    cache = Path(args.cache)
    work = out / 'work'
    sheets = out / 'sheets'
    for d in (cache, sheets, out / 'results'):
        d.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)  # transient G-buffers: written by workers, deleted as soon as consumed
    selftest(out)
    src = load_source(args.source)
    ctx = {'args': args, 'out': str(out.parent if out.name.startswith('controls_') else out)}
    src.prepare(ctx)
    configs = src.configs(ctx)
    tasks = build_tasks(args)
    opt = {'W': W, 'H': H, 'stride': stride, 'samples': args.gt_samples, 'work': str(work), 'cache': str(cache)}
    print('[ref3d] source=%s shaders=%s planner=%s' % (src.name, ctx.get('shader_commit', '-'), ctx.get('plan_source', '-')), flush=True)
    print('[ref3d] GL %s' % ctx.get('renderer', '(no GL)'), flush=True)
    print('[ref3d] %d poses, %dx%d, ground truth %d rays/pixel on a %dx%d grid (stride %d), %d configs' %
          (len(tasks), W, H, args.gt_samples, Wg, Hg, stride, len(configs)), flush=True)
    gt_pool = SPAWN.Pool(args.jobs)
    metric_pool = SPAWN.Pool(max(2, min(8, args.jobs // 3)))
    scenes_used = sorted({s for s, _ in tasks})
    probe_jobs = []
    for s in scenes_used:
        sc = T.get_scene(s)
        for cfg in sorted({p.cfg for pi, p in enumerate(sc.poses) if (s, pi) in set(tasks)}):
            probe_jobs.append(gt_pool.apply_async(T.probe_gt_task, ((s, cfg, opt),)))
    from collections import deque
    todo = iter([(s, p, opt) for s, p in tasks])
    inflight = deque()
    window = max(4, args.jobs)  # bounds the transient G-buffer files on disk to ~window x 26 MB

    def refill():
        while len(inflight) < window:
            t = next(todo, None)
            if t is None:
                break
            inflight.append(gt_pool.apply_async(T.pose_task, (t,)))
    refill()
    plan_text = {}
    if isinstance(src, ScreenSpaceProduction):
        plan_text = {name: src.plan_text(name) for name, _ in configs}
    pending, probe_prod, pose_info = [], {}, {}
    t_gpu = 0.0
    grid = (np.arange(Hg)[:, None] * stride + stride // 2 + 0 * np.arange(Wg)[None, :],
            np.arange(Wg)[None, :] * stride + stride // 2 + 0 * np.arange(Hg)[:, None])
    probe_gt = {}
    for k, (s, pidx) in enumerate(tasks):
        r = inflight.popleft().get()
        refill()
        if not probe_gt:
            for job in probe_jobs:
                sn, cfgi, arr = job.get()
                probe_gt[(sn, cfgi)] = arr
        sc = T.get_scene(s)
        pose = sc.poses[pidx]
        with np.load(r['gb']) as zf_:
            z = {k: zf_[k] for k in zf_.files}
        cam = S.Camera(pose.eye, pose.target, W, H, pose.vfov)
        lights = sc.light_sets[pose.cfg]
        frame = Frame(sc, pose, cam, lights, z['depth32'], z['norm16'], z['zf'], z['surf'], r['gt'], r['probe_xy'],
                      probe_gt[(s, pose.cfg)])
        prods, probes = {}, {}
        t0 = time.time()
        for cfg in configs:
            g, p = src.sampled(frame, cfg, grid, r['probe_xy'])
            prods[cfg[0]] = g
            probes[cfg[0]] = p
        t_gpu += time.time() - t0
        if not args.keep_gbuffers:
            try:
                os.remove(r['gb'])
            except OSError:
                pass
        key = (s, pidx)
        pose_info[key] = {'blockers': r['blockers'], 'probe_prod': probes, 'probe_cls': r['probe_cls']}
        make_sheet = not args.no_sheets
        sheet_path = str(sheets / (pose.id + '.png'))
        bl = '  '.join('%s:%s(%d px)' % (g, v['status'].upper(), v['px']) for g, v in r['blockers'].items())
        summ = 'blockers  %s   |   eye (%.1f,%.1f,%.1f)  light cfg %d' % (bl, *pose.eye, pose.cfg)
        pending.append(metric_pool.apply_async(metrics_task, ((s, pidx, r['gt'], prods, plan_text, sheet_path, stride, summ,
                                                              make_sheet),)))
        if (k + 1) % 10 == 0 or k + 1 == len(tasks):
            print('[ref3d] poses %d/%d  (%.0f s, GPU/source %.1f s)' % (k + 1, len(tasks), time.time() - t_start, t_gpu), flush=True)
    gt_pool.close()
    metrics = {}
    thumbs = {}
    for pend in pending:
        s, pidx, res, th = pend.get()
        metrics[(s, pidx)] = res
        thumbs[(s, pidx)] = th
    metric_pool.close()
    return finish(args, out, src, configs, tasks, pose_info, probe_gt, metrics, ctx, plan_text, t_start, thumbs)


# --------------------------------------------------------------------------------------------- aggregation
KEY_GROUP_EXCLUDE = ('lamp',)


def pose_on_screen(info):
    return all(v['status'] == 'on' for g, v in info['blockers'].items() if g not in KEY_GROUP_EXCLUDE)


def nanmax(xs):
    xs = [x for x in xs if x == x]
    return max(xs) if xs else float('nan')


def nanmean(xs):
    xs = [x for x in xs if x == x]
    return float(np.mean(xs)) if xs else float('nan')


def wmean(pairs):
    num = sum(v * w for v, w in pairs if v == v)
    den = sum(w for v, w in pairs if v == v)
    return num / den if den else float('nan')


# Absolute acceptance (the user's PASS criteria expressed as numbers), evaluated on the ON-SCREEN category only, on the
# worst pose of the scene (pen_cover on the pose-weighted mean). Independent of the regression gates below: a failure here
# is a finding about the production shadows, not a harness fault. edge: 1080p px, p95 of the ground-truth edge points'
# distance to the nearest production edge (0.5 = umbra edge, 0.8 = visible boundary of a soft/thin shadow).
ACCEPT_MAX = {'edge_p95_on': 4.0, 'edge80_p95_on': 6.0, 'mae_on': 0.10, 'leak_on': 0.05, 'false_dark': 0.02,
              'flicker_pop': 0.05, 'pool_dark': 0.02}
ACCEPT_MIN = {'pen_cover_near': 0.70, 'pen_cover_far': 0.70}
# metric name -> aggregate key; regression gates use the unsaturated metrics (edge mean, capped at 64 px) and direction
AGG_KEY = {'mae_on': 'mae_on_worst', 'edge_p95_on': 'edge_p95_worst', 'edge80_p95_on': 'edge80_p95_worst', 'edge_mean_on': 'edge_mean_worst',
           'edge80_mean_on': 'edge80_mean_worst', 'edge_miss': 'edge_miss_worst', 'edge80_miss': 'edge80_miss_worst', 'leak_on': 'leak_on_worst',
           'false_dark': 'false_dark_worst', 'flicker_pop': 'flicker_pop_worst', 'pool_dark': 'pool_dark_worst',
           'pen_cover_near': 'pen_cover_near', 'pen_cover_far': 'pen_cover_far'}
GATE_METRICS = {'mae_on': 'max', 'edge_mean_on': 'max', 'edge80_mean_on': 'max', 'edge_miss': 'max', 'edge80_miss': 'max', 'leak_on': 'max',
                'false_dark': 'max', 'flicker_pop': 'max', 'pool_dark': 'max', 'pen_cover_near': 'min', 'pen_cover_far': 'min'}
GATE_MARGIN = 0.10   # regression thresholds = baseline worst * (1 +/- margin) (+ abs epsilon)
GATE_EPS = 0.005


def gate_limit(scene, cfg, metric):
    """Regression limit for (scene, config, metric). A source whose config is not in the baseline table (a new
    visibility source) is held to the TIGHTEST baseline limit across the production configs."""
    table = GATES.get(scene, {})
    if cfg in table:
        return table[cfg].get(metric)
    vals = [t[metric] for t in table.values() if metric in t]
    if not vals:
        return None
    return min(vals) if GATE_METRICS[metric] == 'max' else max(vals)


def finish(args, out, src, configs, tasks, pose_info, probe_gt, metrics, ctx, plan_text, t_start, thumbs):
    cfg_names = [c[0] for c in configs]
    scenes = sorted({s for s, _ in tasks})
    agg, worst = {}, {}
    lines = []
    P = lines.append
    P('Relight 3D ground-truth harness   source=%s   shaders=%s   planner: %s' % (src.name, ctx.get('shader_commit', '-'), ctx.get('plan_source', '-')))
    P('GL %s | %s px | GT %d rays/px on 1/%d grid | %d poses | %s' % (ctx.get('renderer', '-'), args.res, args.gt_samples, args.stride,
                                                                     len(tasks), time.strftime('%Y-%m-%d %I:%M %p')))
    P('')
    # ---- temporal
    temporal = {}
    for s in scenes:
        sc = T.get_scene(s)
        groups = {}
        for (sn, pidx) in tasks:
            if sn == s:
                groups.setdefault(sc.poses[pidx].group, []).append(pidx)
        for cfgn in cfg_names:
            for gname, idxs in groups.items():
                idxs = sorted(idxs, key=lambda i: sc.poses[i].idx)
                if len(idxs) < 2:
                    continue
                seq = [{'probe': pose_info[(s, i)]['probe_prod'][cfgn], 'cls': pose_info[(s, i)]['probe_cls'], 'cfg': sc.poses[i].cfg} for i in idxs]
                flags = [pose_on_screen(pose_info[(s, i)]) for i in idxs]
                cfg_gt = {sc.poses[i].cfg: probe_gt[(s, sc.poses[i].cfg)] for i in idxs}
                pairs = M.temporal(seq, flags, cfg_gt)
                kind = 'camera' if len({sc.poses[i].cfg for i in idxs}) == 1 else 'light'
                temporal[(s, cfgn, gname)] = {'kind': kind, 'pairs': pairs, 'poses': [sc.poses[i].id for i in idxs]}
    # ---- per scene/config aggregation
    for s in scenes:
        sc = T.get_scene(s)
        keys = [(s, pidx) for (sn, pidx) in tasks if sn == s]
        for cfgn in cfg_names:
            rows = [(sc.poses[p].id, metrics[(s, p)][cfgn], pose_on_screen(pose_info[(s, p)])) for (_, p) in keys]
            on_rows = [r for r in rows if r[1]['n_on'] >= 50]
            a = {'poses': len(rows), 'poses_onscreen': sum(1 for r in rows if r[2])}
            a['mae_on_mean'] = wmean([(r[1]['mae_on'], r[1]['n_on']) for r in on_rows])
            a['mae_on_worst'], w = max(((r[1]['mae_on'], r[0]) for r in on_rows), default=(float('nan'), ''))
            worst[(s, cfgn, 'mae_on')] = w
            e_rows = [r for r in rows if r[1]['edge_n_on'] >= 30]
            a['edge_p50_on'] = nanmean([r[1]['edge_p50_on'] for r in e_rows])
            a['edge_p95_worst'], w = max(((r[1]['edge_p95_on'], r[0]) for r in e_rows if r[1]['edge_p95_on'] == r[1]['edge_p95_on']),
                                         default=(float('nan'), ''))
            worst[(s, cfgn, 'edge_p95_on')] = w
            a['edge_miss_worst'] = nanmax([r[1]['edge_miss_on'] for r in e_rows])
            a['edge_mean_worst'] = nanmax([r[1]['edge_mean_on'] for r in e_rows])
            e8_rows = [r for r in rows if r[1]['edge80_n_on'] >= 30]
            a['edge80_p50_on'] = nanmean([r[1]['edge80_p50_on'] for r in e8_rows])
            a['edge80_p95_worst'], w = max(((r[1]['edge80_p95_on'], r[0]) for r in e8_rows if r[1]['edge80_p95_on'] == r[1]['edge80_p95_on']),
                                           default=(float('nan'), ''))
            worst[(s, cfgn, 'edge80_p95_on')] = w
            a['edge80_miss_worst'] = nanmax([r[1]['edge80_miss_on'] for r in e8_rows])
            a['edge80_mean_worst'] = nanmax([r[1]['edge80_mean_on'] for r in e8_rows])
            tot = np.sum([r[1]['n_cls'] for r in rows], axis=0)
            a['cls_share'] = [float(x) / max(1, int(tot.sum())) for x in tot]
            a['edge_false_worst'] = nanmax([r[1]['edge_false'] for r in e_rows if r[1]['edge_false_n'] >= 30])
            l_rows = [r for r in rows if r[1]['n_on'] >= 50]
            a['leak_on_worst'], w = max(((r[1]['leak_on'], r[0]) for r in l_rows if r[1]['leak_on'] == r[1]['leak_on']), default=(float('nan'), ''))
            worst[(s, cfgn, 'leak_on')] = w
            a['false_dark_worst'], w = max(((r[1]['false_dark'], r[0]) for r in rows if r[1]['false_dark'] == r[1]['false_dark']), default=(float('nan'), ''))
            worst[(s, cfgn, 'false_dark')] = w
            a['false_dark_mean'] = nanmean([r[1]['false_dark'] for r in rows])
            a['pen_ratio_med'] = float(np.nanmedian([r[1]['pen_ratio'] for r in rows])) if any(r[1]['pen_ratio'] == r[1]['pen_ratio'] for r in rows) else float('nan')
            a['pen_cover_near'] = wmean([(r[1]['pen_cover_near'], r[1]['pen_n_near']) for r in rows])
            a['pen_cover_far'] = wmean([(r[1]['pen_cover_far'], r[1]['pen_n_far']) for r in rows])
            a['pen_bias_near'] = wmean([(r[1]['pen_bias_near'], r[1]['pen_n_near']) for r in rows])
            a['pen_bias_far'] = wmean([(r[1]['pen_bias_far'], r[1]['pen_n_far']) for r in rows])
            x_rows = [r for r in rows if r[1]['n_exp'] >= 50]
            a['mae_exp_mean'] = wmean([(r[1]['mae_exp'], r[1]['n_exp']) for r in x_rows])
            a['leak_exp_mean'] = wmean([(r[1]['leak_exp'], r[1]['n_exp']) for r in x_rows])
            tot_sh = sum(r[1]['n_shadow'] for r in rows)
            a['exp_share'] = sum(r[1]['n_exp'] for r in rows) / tot_sh if tot_sh else float('nan')
            if s == 'C':
                a['pool_dark_worst'], w = max(((r[1]['pool_dark'], r[0]) for r in rows if r[1].get('pool_dark', float('nan')) == r[1].get('pool_dark', float('nan'))),
                                              default=(float('nan'), ''))
                worst[(s, cfgn, 'pool_dark')] = w
                a['pool_prod_min_worst'] = min((r[1]['pool_prod_min'] for r in rows if r[1].get('pool_prod_min', float('nan')) == r[1].get('pool_prod_min', float('nan'))), default=float('nan'))
            # temporal: camera-only sequences; unexpected = every shadowing blocker on-screen in both poses
            fl, flx, fl_light = [], [], []
            for (sn, cn, gname), tr in temporal.items():
                if sn != s or cn != cfgn:
                    continue
                for pr in tr['pairs']:
                    name = tr['poses'][pr['i']] + '->' + tr['poses'][pr['i'] + 1]
                    if 'p95' in pr:
                        (fl if tr['kind'] == 'camera' else fl_light).append((pr['p95'], name, pr['mean'], pr['pop']))
                    if 'x_p95' in pr and tr['kind'] == 'camera':
                        flx.append((pr['x_p95'], name, pr['x_mean'], pr['x_pop']))
            a['flicker_p95_worst'], w = max(((x[0], x[1]) for x in fl), default=(float('nan'), ''))
            worst[(s, cfgn, 'flicker')] = w
            a['flicker_mean'] = float(np.mean([x[2] for x in fl])) if fl else float('nan')
            a['flicker_pop_worst'], w = max(((x[3], x[1]) for x in fl), default=(float('nan'), ''))
            worst[(s, cfgn, 'flicker_pop')] = w
            a['flicker_x_p95_worst'] = max((x[0] for x in flx), default=float('nan'))
            a['flicker_x_mean'] = float(np.mean([x[2] for x in flx])) if flx else float('nan')
            a['lightseq_flicker_p95_worst'] = max((x[0] for x in fl_light), default=float('nan'))
            agg[(s, cfgn)] = a
    # ---- regression gates (thresholds baked from the HEAD 4dd9ff661c1 baseline, see GATES / GATE_NOTES)
    failures, gate_rows = [], []
    for s in scenes:
        for cfgn in cfg_names:
            a = agg[(s, cfgn)]
            for metric, direction in GATE_METRICS.items():
                key = AGG_KEY[metric]
                limit = gate_limit(s, cfgn, metric)
                v = a.get(key, float('nan'))
                if limit is None or v != v:
                    continue
                bad = v > limit if direction == 'max' else v < limit
                gate_rows.append((s, cfgn, metric, v, limit, 'FAIL' if bad else 'ok'))
                if bad:
                    failures.append((s, cfgn, metric, v, limit))
    partial = bool(args.limit or args.pose_filter) and not getattr(args, 'force_gates', False)
    if partial:  # the limits are worst-case over the FULL pose set of each scene; a subset says nothing about them
        gate_rows, failures = [], []
    if args.print_gates:
        table = {}
        for s in scenes:
            for cfgn in cfg_names:
                a = agg[(s, cfgn)]
                for metric, direction in GATE_METRICS.items():
                    v = a.get(AGG_KEY[metric], float('nan'))
                    if v != v:
                        continue
                    lim = v * (1 + GATE_MARGIN) + GATE_EPS if direction == 'max' else max(0.0, v * (1 - GATE_MARGIN) - GATE_EPS)
                    table.setdefault(s, {}).setdefault(cfgn, {})[metric] = round(lim, 4)
        (out / 'gates_proposed.json').write_text(json.dumps(table, indent=1), encoding='utf-8')
    # ---- summary text
    P('=== per scene / quality config: UNEXPECTED category (blocker on-screen) ===')
    hdr = '%-3s %-15s %5s %-22s %8s %8s %8s %8s %8s %8s %8s %8s' % ('sc', 'config', 'poses', 'plan', 'mae_on', 'mae_wst', 'edge50', 'edge95w', 'missw', 'leak_w', 'acne_w', 'flick95w')
    P(hdr)
    for s in scenes:
        for cfgn in cfg_names:
            a = agg[(s, cfgn)]
            P('%-3s %-15s %5d %-22s %8s %8s %8s %8s %8s %8s %8s %8s' % (s, cfgn, a['poses'], plan_text.get(cfgn, '')[:22], fmt(a['mae_on_mean']), fmt(a['mae_on_worst']),
                                                                    fmt(a['edge_p50_on'], 1), fmt(a['edge_p95_worst'], 1), fmt(a['edge_miss_worst'], 2), fmt(a['leak_on_worst']),
                                                                    fmt(a['false_dark_worst']), fmt(a['flicker_p95_worst'])))
    P('  mae_on: |prod-GT| visibility on shadowed pixels whose blocker is on-screen (mean over poses / worst pose); edge50/95w: shadow-edge '
      'location error in 1080p px (median / worst pose p95, capped at 64); missw: worst pose fraction of GT edge points with no production edge within 12 px; leak_w: worst pose fraction of dark GT pixels left lit (>0.75); acne_w: worst pose fraction of GT-lit pixels darkened (<0.75); '
      'flick95w: worst camera-only adjacent-pose p95 change of the same world floor point beyond ground-truth change.')
    P('')
    P('=== penumbra (on-screen blockers): share of GT penumbra pixels (0.1-0.9) that production also shows as penumbra; mean(prod-GT) there ===')
    P('%-3s %-15s %8s %8s %9s %9s %9s' % ('sc', 'config', 'cov_near', 'cov_far', 'bias_near', 'bias_far', 'pen_ratio'))
    for s in scenes:
        for cfgn in cfg_names:
            a = agg[(s, cfgn)]
            P('%-3s %-15s %8s %8s %9s %9s %9s' % (s, cfgn, fmt(a['pen_cover_near'], 2), fmt(a['pen_cover_far'], 2), fmt(a['pen_bias_near']), fmt(a['pen_bias_far']), fmt(a['pen_ratio_med'], 2)))
    P('  near = blocker within 0.6 m of the receiver, far = beyond 1.0 m (penumbra should be sharp near, soft far).')
    P('')
    P('=== EXPECTED screen-space failures (blocker off-screen / hidden / absent in the G-buffer): reported, not gated ===')
    P('    shadowed-pixel shares by blocker category (identical for every config):')
    for s in scenes:
        cs = agg[(s, cfg_names[0])]['cls_share']
        P('    %-3s on-screen %5.1f%%   off-screen %5.1f%%   hidden behind a thicker surface %5.1f%%   absent from G-buffer %5.1f%%' % (s, *[100 * x for x in cs]))
    P('%-3s %-15s %9s %9s %9s' % ('sc', 'config', 'share', 'mae_exp', 'leak_exp'))
    for s in scenes:
        for cfgn in cfg_names:
            a = agg[(s, cfgn)]
            P('%-3s %-15s %8.0f%% %9s %8s' % (s, cfgn, 100 * a['exp_share'] if a['exp_share'] == a['exp_share'] else float('nan'), fmt(a['mae_exp_mean']),
                                              ('%.0f%%' % (100 * a['leak_exp_mean'])) if a['leak_exp_mean'] == a['leak_exp_mean'] else ' n/a'))
    P('  share = fraction of GT-shadowed pixels whose blocker screen-space cannot see; leak_exp = of those that are fully dark in GT, the fraction left lit.')
    P('')
    if 'C' in scenes:
        P('=== scene C lamp pool (door + wall within 1.8 m of the housed lamp; GT = lit) ===')
        for cfgn in cfg_names:
            a = agg[('C', cfgn)]
            P('  %-15s worst-pose dark fraction %s  (pose %s)   min visibility seen %s' % (cfgn, fmt(a.get('pool_dark_worst')), worst.get(('C', cfgn, 'pool_dark'), ''), fmt(a.get('pool_prod_min_worst'))))
        P('')
    P('=== acceptance (the user PASS criteria as numbers; ON-SCREEN blockers, worst pose): umbra edge p95<=%.0fpx, soft edge p95<=%.0fpx, mae<=%.2f, leak<=%.0f%%, acne<=%.0f%%, '
      'flicker pop<=%.0f%%, penumbra coverage>=%.0f%%, lamp pool dark<=%.0f%% ===' % (
          ACCEPT_MAX['edge_p95_on'], ACCEPT_MAX['edge80_p95_on'], ACCEPT_MAX['mae_on'], 100 * ACCEPT_MAX['leak_on'], 100 * ACCEPT_MAX['false_dark'],
          100 * ACCEPT_MAX['flicker_pop'], 100 * ACCEPT_MIN['pen_cover_near'], 100 * ACCEPT_MAX['pool_dark']))
    accept = {}
    for s in scenes:
        for cfgn in cfg_names:
            a = agg[(s, cfgn)]
            vals = {m: a.get(AGG_KEY[m], float('nan')) for m in list(ACCEPT_MAX) + list(ACCEPT_MIN)}
            bad = [k for k in ACCEPT_MAX if vals[k] == vals[k] and vals[k] > ACCEPT_MAX[k]] + \
                  [k for k in ACCEPT_MIN if vals[k] == vals[k] and vals[k] < ACCEPT_MIN[k]]
            accept[(s, cfgn)] = bad
            P('  %-3s %-15s %s%s' % (s, cfgn, 'PASS' if not bad else 'FAIL  ', '  '.join('%s=%s' % (k, fmt(vals[k], 2)) for k in bad)))
    P('')
    P('=== worst on-screen poses per scene (config %s) ===' % cfg_names[-1])
    for s in scenes:
        sc = T.get_scene(s)
        rows = sorted(((metrics[(s, p)][cfg_names[-1]]['mae_on'], sc.poses[p].id, p) for (sn, p) in tasks if sn == s and metrics[(s, p)][cfg_names[-1]]['n_on'] >= 50), reverse=True)[:4]
        for v, pid, p in rows:
            m = metrics[(s, p)][cfg_names[-1]]
            P('  %-26s mae_on %.3f edge95 %s leak %s acne %s  sheet sheets/%s.png' % (pid, v, fmt(m['edge_p95_on'], 1), fmt(m['leak_on'], 2), fmt(m['false_dark'], 3), pid))
    P('')
    P('=== gates (regression thresholds, unexpected category only) ===')
    import textwrap
    for ln in textwrap.wrap(GATE_NOTES, 150):
        P('  ' + ln)
    if not gate_rows:
        P('  (no thresholds configured)')
    for row in gate_rows:
        if row[5] == 'FAIL':
            P('  FAIL %-3s %-15s %-12s %.4f > %.4f' % (row[0], row[1], row[2], row[3], row[4]))
    P('  %d gate checks, %d failures%s' % (len(gate_rows), len(failures), '  (partial pose set: gates skipped)' if partial else ''))
    # ---- per pose table + files
    P('')
    P('=== per pose: blocker visibility (ON/OFF/HIDDEN by key group) and metrics ===')
    csv = ['scene,pose,tag,onscreen,blockers,config,n_cand,n_shadow,n_on,n_exp,mae_on,mae_exp,lit_mae,false_dark,leak_on,leak_exp,edge_p50_on,edge_p95_on,edge_miss_on,edge_false,pen_ratio,pool_dark']
    for s in scenes:
        sc = T.get_scene(s)
        for (sn, p) in tasks:
            if sn != s:
                continue
            pose = sc.poses[p]
            info = pose_info[(s, p)]
            bl = ' '.join('%s=%s' % (g, v['status']) for g, v in info['blockers'].items())
            for cfgn in cfg_names:
                m = metrics[(s, p)][cfgn]
                csv.append(','.join(str(x) for x in (s, pose.id, pose.tag, int(pose_on_screen(info)), bl.replace(' ', ';'), cfgn, m['n_cand'], m['n_shadow'], m['n_on'], m['n_exp'],
                                                    '%.4f' % m['mae_on'], '%.4f' % m['mae_exp'], '%.4f' % m['lit_mae'], '%.4f' % m['false_dark'], '%.4f' % m['leak_on'], '%.4f' % m['leak_exp'],
                                                    '%.2f' % m['edge_p50_on'], '%.2f' % m['edge_p95_on'], '%.3f' % m['edge_miss_on'], '%.3f' % m['edge_false'], '%.3f' % m['pen_ratio'],
                                                    '%.4f' % m.get('pool_dark', float('nan')))))
            m = metrics[(s, p)][cfg_names[-1]]
            P('  %-26s %-9s %-52s on:%5.0f%% exp:%5.0f%%  [%s] mae_on %s edge95 %s' % (
                pose.id, pose.tag, bl, 100 * m['n_on'] / max(1, m['n_shadow']), 100 * m['n_exp'] / max(1, m['n_shadow']), cfg_names[-1], fmt(m['mae_on']), fmt(m['edge_p95_on'], 1)))
    P('')
    P('=== temporal: the same world floor point in adjacent poses; flicker = |prod change - GT change| (probe points visible in both) ===')
    P('    unexpected = every shadowing blocker on-screen in both poses; expected = a blocker left / hid (screen-space failure, not gated)')
    for (s, cfgn, gname), tr in sorted(temporal.items()):
        ps = [p for p in tr['pairs'] if 'p95' in p]
        xs = [p for p in tr['pairs'] if 'x_p95' in p]
        if not ps and not xs:
            continue
        P('  %-2s %-15s %-22s %-6s pairs %2d  unexpected: p95 worst %s mean %s pop %s | expected: p95 worst %s mean %s' % (
            s, cfgn, gname, tr['kind'], len(tr['pairs']),
            fmt(max((p['p95'] for p in ps), default=float('nan'))), fmt(float(np.mean([p['mean'] for p in ps])) if ps else float('nan')),
            fmt(max((p['pop'] for p in ps), default=float('nan'))),
            fmt(max((p['x_p95'] for p in xs), default=float('nan'))), fmt(float(np.mean([p['x_mean'] for p in xs])) if xs else float('nan'))))
    P('')
    if not args.no_sheets:
        import relight_3d_sheets as SH
        for s in scenes:
            sc = T.get_scene(s)
            for cfgn in sorted({cfg_names[0], cfg_names[-1]}):
                tiles = []
                for (sn, p) in tasks:
                    if sn != s:
                        continue
                    m = metrics[(s, p)][cfgn]
                    gi, pi = thumbs[(s, p)][cfgn]
                    tiles.append(('%s mae %s' % (sc.poses[p].id[2:], fmt(m['mae_on'], 2)), gi, pi))
                path = out / 'sheets' / ('overview_%s_%s.png' % (s, cfgn.replace('+', '_')))
                SH.overview_sheet(path, '%s overview, config %s: ground truth | production (per pose, 2x reduced)' % (sc.description, cfgn), tiles)
                P('overview sheet: %s' % path)
    P('')
    P('elapsed %.0f s' % (time.time() - t_start))
    text = '\n'.join(lines)
    (out / 'summary.txt').write_text(text, encoding='utf-8')
    (out / 'poses.csv').write_text('\n'.join(csv), encoding='utf-8')
    payload = {'agg': {'%s|%s' % k: v for k, v in agg.items()}, 'worst': {'%s|%s|%s' % k: v for k, v in worst.items()},
               'accept_fail': {'%s|%s' % k: v for k, v in accept.items()},
               'gate_failures': len(failures), 'source': src.name, 'shader_commit': ctx.get('shader_commit', '-')}
    (out / 'results.json').write_text(json.dumps(payload, indent=1, default=str), encoding='utf-8')
    print(text, flush=True)
    return {'gate_failures': len(failures), 'accept_failures': sum(1 for v in accept.values() if v), 'accept': accept, 'agg': agg, 'strict': args.strict}


def finish_gate_exit(result):
    return 1 if result['gate_failures'] else 0


if __name__ == '__main__':
    mp.freeze_support()
    code = main()
    if isinstance(code, dict):
        code = 1 if (code['gate_failures'] or (code['strict'] and code['accept_failures'])) else 0
    sys.exit(code)
