"""Scene W / WS report: the lamp-in-a-shade corner. Region metrics and 4-column contact sheets.

    py -3 -I scripts/code_tools/relight_3d_corner_report.py --out .tmp/cube_r1/sheets_W

For every pose of W (open-bottom shade) and WS (sealed box housing, open bottom face) it renders the ground truth and four visibility sources:
  screen      today's screen-space soft shadows (the Phase 0 commit's production shaders, Default plan, Light bias 25 cm);
  screen_bias005  the same with the Light bias at 5 cm (the housing exclusion of the screen trace);
  cube        the world cube maps, face near plane 25 cm (max(5 cm, Light bias));
  cube_n005   the world cube maps with the face near plane AND the screen trace's housing bias at 5 cm (diagnostic);
  cube_n010   the same at 10 cm.
Region metrics (grid pixels of the ground-truth G-buffer, per pose with >= 150 region pixels, pixel-weighted over poses):
  wall_above  the wall above the lamp (|x| < 1.0, 2.40 < z < 3.30 on the y = 5.0 wall): the ground truth is DARK here (the shade blocks upward light);
              reported: mean visibility (GT vs source), MAE, and the share of GT-dark (< 0.5) pixels the source leaves lit (> 0.75);
  vent        the LOWER louvered vent on the side wall (slats and the wall behind them, y 4.0..4.7, z 1.0..1.45; the open-bottom shade lights it):
              reported: MAE, and the visibility CONTRAST (standard deviation over the region, GT vs source) = how much of the louver shadow survives;
  vent_hi     the upper vent (z 2.0..2.45, level with the lamp: the shade hood blocks it in the ground truth).
"""
import argparse
import sys
import types
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

import relight_3d_gl as G  # noqa: E402
import relight_3d_plan as PL  # noqa: E402
import relight_3d_scene as S  # noqa: E402
import relight_3d_sheets as SH  # noqa: E402
import relight_3d_tasks as T  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
BASE = 'e150b927cf4'
SOURCES = ['screen', 'screen_bias005', 'cube', 'cube_near005', 'cube2_near010']
LABELS = {'screen': 'screen-space (today)', 'screen_bias005': 'screen-space, Light bias 5 cm', 'cube': 'cube maps (near 25 cm)', 'cube_near005': 'cube maps, near 5 cm', 'cube2_near010': 'cube maps, near 10 cm'}
SHEET_POSES = {'W': ['W_orbit_low_002', 'W_orbit_low_007', 'W_orbit_eye_005', 'W_dolly_004', 'W_vent_002', 'W_vent_006'],
               'WS': ['WS_orbit_low_004', 'WS_orbit_eye_005', 'WS_vent_004']}


def regions(pos, surf, names):
    x, y, z = pos[..., 0], pos[..., 1], pos[..., 2]
    wall = (np.abs(y - 5.0) < 0.03) & (np.abs(x) < 1.0) & (z > 2.40) & (z < 3.30)
    ids = [i for i, n in enumerate(names) if n.startswith('vent') or n.startswith('vlo') or n == 'wall_side']
    vent_hi = np.isin(surf, ids) & (y > 4.0) & (y < 4.7) & (z > 2.0) & (z < 2.45)
    vent = np.isin(surf, ids) & (y > 4.0) & (y < 4.7) & (z > 1.0) & (z < 1.45)
    return {'wall_above': wall, 'vent': vent, 'vent_hi': vent_hi}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scenes', default='W,WS')
    ap.add_argument('--out', default=str(REPO / '.tmp' / 'cube_r1' / 'sheets_W'))
    ap.add_argument('--cache', default=str(REPO / '.tmp' / 'ref3d_cache'))
    ap.add_argument('--scale', type=float, default=1.5)
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    import importlib.util
    spec = importlib.util.spec_from_file_location('cube_source', str(Path(__file__).with_name('relight_3d_cube_source.py')))
    mod = importlib.util.module_from_spec(spec)
    sys.modules['__main__'].VisibilitySource = object
    spec.loader.exec_module(mod)
    cube = mod.CubeSource()
    cube.prepare({'args': types.SimpleNamespace(shader_ref='WORKTREE'), 'out': str(REPO / '.tmp' / 'cube_r1' / 'cornerout')})
    cfgs = {c[0]: c for c in cube.CONFIGS}
    cube.enabled = [cfgs[n] for n in SOURCES[2:]]
    cube_cfgs = dict((c[0], c) for c in cube.configs({}))
    base_tree = G.SourceTree(BASE)
    base_runner = G.AreaRunner(base_tree, gl=cube.runner.gl)
    plans_base, _ = PL.compute_plans(base_tree, REPO / '.tmp' / 'cube_r1')
    opt = {'W': 1920, 'H': 1080, 'stride': 4, 'samples': 256, 'work': str(REPO / '.tmp' / 'cube_r1' / 'cornerwork'), 'cache': args.cache}
    Hg, Wg = 270, 480
    gj, gi = np.mgrid[0:Hg, 0:Wg]
    fj, fi = gj * 4 + 2, gi * 4 + 2
    lines = []
    P = lambda t='': (lines.append(t), print(t, flush=True))
    for name in args.scenes.split(','):
        sc = T.get_scene(name)
        names = [p.name for p in sc.prims]
        acc = {(src, reg): [] for src in SOURCES for reg in ('wall_above', 'vent', 'vent_hi')}
        for pidx, pose in enumerate(sc.poses):
            r = T.pose_task((name, pidx, opt))
            with np.load(r['gb']) as z:
                depth32, norm16 = z['depth32'], z['norm16']
            cam = S.Camera(pose.eye, pose.target, 1920, 1080, pose.vfov)
            lights = sc.light_sets[pose.cfg]
            frame = types.SimpleNamespace(scene=sc, pose=pose, cam=cam, lights=lights, depth32=depth32, norm16=norm16)
            prods = {'screen': base_runner.run(cam, depth32, norm16, lights, plans_base['Default'][len(lights)])[:, fj, fi]}
            prods['screen_bias005'] = base_runner.run(cam, depth32, norm16, lights, plans_base['Default'][len(lights)], light_bias=0.05)[:, fj, fi]
            for src in SOURCES[2:]:
                prods[src] = cube.visibility(frame, cube_cfgs[src])[:, fj, fi]
            gt = r['gt']
            L = gt['lights'][0]
            regs = regions(gt['pos'], gt['surf'], names)
            for reg, mask in regs.items():
                m = mask & L['cand']
                if m.sum() < 150:
                    continue
                g = L['vis'][m]
                for src in SOURCES:
                    v = prods[src][0][m]
                    dark = g < 0.5
                    acc[(src, reg)].append((int(m.sum()), float(g.mean()), float(v.mean()), float(np.abs(v - g).mean()), float(g.std()), float(v.std()),
                                            float((v[dark] > 0.75).mean()) if dark.any() else float('nan')))
            if pose.id in SHEET_POSES.get(name, []):
                albedo = [p.albedo for p in sc.prims]
                gt_vis = [Lg['vis'] * Lg['cand'] + (~Lg['cand']) for Lg in gt['lights']]
                gt_img = SH.shade(gt, gt_vis, albedo)
                k = args.scale
                def up(img):
                    im = SH.to_image(img)
                    return im.resize((int(im.width * k), int(im.height * k)), Image.BICUBIC)
                pg = up(gt_img)
                w, h = pg.size
                gap, head = 8, 64
                sheet = Image.new('RGB', (4 * w + 5 * gap, head + 2 * h + 3 * gap), (18, 18, 20))
                dr = ImageDraw.Draw(sheet)
                dr.text((gap, 6), '%s  [%s]' % (pose.id, sc.description), fill=(240, 240, 240), font=SH._font(18))
                dr.text((gap, 30), 'eye (%.1f, %.1f, %.1f) target (%.1f, %.1f, %.1f); columns: ground truth | screen-space (today) | cube maps (near 25 cm) | cube maps, near 5 cm; '
                        'bottom: error views (red = left lit, blue = false dark)' % (*pose.eye, *pose.target), fill=(220, 220, 170), font=SH._font(13))
                cols = [('ground truth (ray traced)', gt_img, SH.diff_image(gt, [gt['lights'][0]['vis']], gt_img))]
                for src in ('screen', 'cube', 'cube_near005'):
                    Lg = gt['lights'][0]
                    msk = Lg['cand'] & (Lg['vis'] < 0.98)
                    mae = float(np.abs(prods[src][0] - Lg['vis'])[msk].mean()) if msk.any() else float('nan')
                    cols.append(('%s   mae %.3f' % (LABELS[src], mae), SH.shade(gt, prods[src], albedo), SH.diff_image(gt, prods[src], gt_img)))
                for ci, (label, top, err) in enumerate(cols):
                    x = gap + ci * (w + gap)
                    dr.text((x, head - 16), label, fill=(150, 200, 150), font=SH._font(13))
                    sheet.paste(up(top), (x, head))
                    sheet.paste(up(err * 1.0 if ci else gt_img * 0.35), (x, head + h + gap))
                sheet.save(out / ('%s.png' % pose.id), compress_level=4)
            Path(r['gb']).unlink(missing_ok=True)
        P('=== scene %s: %s (%d poses) ===' % (name, sc.description, len(sc.poses)))
        for reg in ('wall_above', 'vent', 'vent_hi'):
            P('  region %s (per-pose >= 150 px, pixel-weighted over poses; GT mean/std vs source)' % reg)
            P('    %-16s %5s %9s %9s %7s %9s %9s %12s' % ('source', 'poses', 'GT mean', 'src mean', 'MAE', 'GT std', 'src std', 'GT-dark lit'))
            for src in SOURCES:
                rows = acc[(src, reg)]
                if not rows:
                    P('    %-16s no pose sees the region' % src)
                    continue
                n = np.array([r_[0] for r_ in rows], float)
                wavg = lambda i: float(np.nansum([r_[i] * r_[0] for r_ in rows]) / max(1.0, sum(r_[0] for r_ in rows if r_[i] == r_[i])))
                P('    %-16s %5d %9.3f %9.3f %7.3f %9.3f %9.3f %11.0f%%' % (src, len(rows), wavg(1), wavg(2), wavg(3), wavg(4), wavg(5), 100 * wavg(6)))
    (out / 'region_report.txt').write_text('\n'.join(lines), encoding='utf-8')


if __name__ == '__main__':
    main()
