"""GPU cost of the P1-lite world resolve (production gather + filter + upsample on the hidden WGL context, GL timestamp queries).

    py -3 -I scripts/code_tools/relight_cost_check.py --world

Four lights (K4, Medium preset, Thin OFF, the production planner of each tree at the target size) on the harness sphere scene, 1080p and 4K.
  base    the base commit's programs (P0, no world code), all four lights on the screen path;
  worldN  the WORKTREE programs with the first N lights world-resident (the real CPU-raster atlas of the scene), N = 1, 2;
  worst2  as world2 with an atlas of near blockers everywhere: every pixel runs the full 16 + 32 gathers (an upper bound).
Reported: whole area pass ms (gather + blur + upsample draws, GPU time only), the upsample pass alone, and the RESOLVE = the
upsample with world minus the upsample without. Hard ceilings: the area pass at Medium 1080p K4 with N = 2 is at most 2.5 x the
base pass (the user's ceiling), and the resolve at 4K N = 2 is at most 3 ms. The raster cost is structural (6 x N face passes,
never demoted per frame; Tracy in the viewer is report-only) and is printed from the pure ledger.
"""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import numpy as np  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
BASE = 'e150b927cf4'
RATIO_CEILING = 2.5
RESOLVE_CEILING_MS = 3.0


def best(fn, repeats=12):
    fn()  # warm-up
    return min(fn() for _ in range(repeats))


def main():
    import relight_3d_gl as G
    import relight_3d_plan as PL
    import relight_3d_scene as S
    import relight_3d_cube_source as CS
    out = REPO / '.tmp' / 'cube_r1'
    base_tree, work_tree = G.SourceTree(BASE), G.SourceTree('WORKTREE')
    base_runner = G.AreaRunner(base_tree)
    work_runner = G.AreaRunner(work_tree, gl=base_runner.gl)
    print('[world-cost] GL %s' % base_runner.gl.renderer, flush=True)
    sc = S.scene_sphere()
    lights = [S.Light('point', (2.0 * np.cos(t), 2.0 * np.sin(t), 1.6), 0.1, 10.0, seed=S.fnv_seed('B:%d' % i))
              for i, t in enumerate(np.radians([45.0, 135.0, 225.0, 315.0]))]
    cache_dir = out / 'cube_cache'
    near, far = CS.near_plane(), 10.0
    atlas = np.ones((CS.ATLAS_H_PER_LIGHT * 2, CS.ATLAS_W), np.float32)
    for k in range(2):
        tiles = CS.light_tiles(sc, lights[k], near, far, cache_dir)
        for f in range(6):
            atlas[(2 * k + f // 3) * CS.TILE:(2 * k + f // 3 + 1) * CS.TILE, (f % 3) * CS.TILE:(f % 3 + 1) * CS.TILE] = tiles[f]
    worst_atlas = np.zeros_like(atlas)
    rows = []
    for label, W, H in (('1080p', 1920, 1080), ('4K', 3840, 2160)):
        t0 = time.time()
        cam = S.Camera((0, -3.4, 1.8), (0, 0, 0.25), W, H)
        g = S.cast_primary(sc, cam)
        _, d32, n16 = S.encode_gbuffer(cam, g)
        plans_base, _ = PL.compute_plans(base_tree, out, W, H)
        plans_work, _ = PL.compute_plans(work_tree, out, W, H)
        assert plans_base == plans_work
        plan = plans_work['Default'][4]
        scale = H / 1080.0

        def run(runner, world):
            st = {}
            runner.run(cam, d32, n16, lights, plan, scale=scale, world=world, stats=st)
            return st

        def channels(n):
            return [{'row': 2 * k, 'near': near, 'far': far, 'r_map': 0.1, 'contact': CS.contact_distance(far), 'seed': int(lights[k].seed)} if k < n else None
                    for k in range(4)]
        variants = {'base': (base_runner, None), 'world1': (work_runner, {'atlas': atlas, 'channels': channels(1)}),
                    'world2': (work_runner, {'atlas': atlas, 'channels': channels(2)}),
                    'worst2': (work_runner, {'atlas': worst_atlas, 'channels': channels(2)})}
        res = {}
        for name, (runner, world) in variants.items():
            tot = best(lambda: sum(run(runner, world).values()))
            ups = best(lambda: run(runner, world)['upsample_ms'])
            res[name] = (tot, ups)
        base_tot, base_ups = res['base']
        print('[world-cost] %s %dx%d plan %d rays x %d steps %s: base area pass %.3f ms (upsample %.3f)' % (
            label, W, H, plan['samples'], plan['steps'], 'half' if plan['half'] else 'full', base_tot, base_ups), flush=True)
        for name in ('world1', 'world2', 'worst2'):
            tot, ups = res[name]
            resolve = ups - base_ups
            print('[world-cost] %s %-6s area pass %.3f ms (%.2fx base) | upsample %.3f ms | RESOLVE %.3f ms' % (
                label, name, tot, tot / base_tot, ups, resolve), flush=True)
            rows.append((label, name, tot / base_tot, resolve))
        print('[world-cost] %s raster: %d face passes at N=2 (6 per light, structural), atlas %.2f MiB for 2 lights; %.0f s' % (
            label, 12, 2 * 6.7734, time.time() - t0), flush=True)
    r1080 = [r for r in rows if r[0] == '1080p' and r[1] == 'world2'][0]
    r4k = [r for r in rows if r[0] == '4K' and r[1] == 'world2'][0]
    r4kw = [r for r in rows if r[0] == '4K' and r[1] == 'worst2'][0]
    assert r1080[2] <= RATIO_CEILING, ('area pass at Medium 1080p K4 N=2 exceeds %.1fx the base pass' % RATIO_CEILING, r1080)
    assert r4k[3] <= RESOLVE_CEILING_MS and r4kw[3] <= RESOLVE_CEILING_MS, ('resolve at 4K N=2 exceeds %.1f ms' % RESOLVE_CEILING_MS, r4k, r4kw)
    print('[world-cost] PASS: area pass %.2fx base at Medium 1080p K4 N=2 (ceiling %.1fx); resolve at 4K N=2 %.3f ms (worst case %.3f ms; ceiling %.1f ms)' % (
        r1080[2], RATIO_CEILING, r4k[3], r4kw[3], RESOLVE_CEILING_MS), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
