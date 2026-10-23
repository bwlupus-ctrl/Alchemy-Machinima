"""Side-by-side contact sheets for the world cube maps: ground truth | today's screen-space | cube maps (+ error views).

    py -3 -I scripts/code_tools/relight_3d_cube_sheets.py --scenes A2,A --out .tmp/cube_r1/sheets_A2

Each sheet is one pose, rendered at 2x the harness grid (960 x 540 per panel): top row ground truth | screen-space (the base commit's
production shaders, Default plan) | cube maps (WORKTREE shaders, N = 1); bottom row the same pair as error views (red = left lit where the ground
truth is dark, blue = darkened where the ground truth is lit) plus the ground truth's own unshadowed context. The CPU ray-cast raster stands in
for the viewer's shadow pass (see relight_3d_cube_source.py).
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
BASE = 'b0bb2d24f18'
POSES = {
    'A2': ['A2_orbit_eye_near_000', 'A2_orbit_eye_near_006', 'A2_orbit_eye_far_003', 'A2_orbit_mid_near_002', 'A2_orbit_mid_far_009',
           'A2_orbit_high_near_005'],
    'A': ['A_orbit_eye_near_000', 'A_orbit_eye_near_006', 'A_orbit_eye_far_003', 'A_orbit_mid_near_002', 'A_orbit_mid_far_009',
          'A_orbit_high_near_005'],
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scenes', default='A2,A')
    ap.add_argument('--out', default=str(REPO / '.tmp' / 'cube_r1' / 'sheets_A2'))
    ap.add_argument('--cache', default=str(REPO / '.tmp' / 'ref3d_cache'))
    ap.add_argument('--scale', type=int, default=2)
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    import importlib.util
    spec = importlib.util.spec_from_file_location('cube_source', str(Path(__file__).with_name('relight_3d_cube_source.py')))
    mod = importlib.util.module_from_spec(spec)
    sys.modules['__main__'].VisibilitySource = object
    spec.loader.exec_module(mod)
    cube = mod.CubeSource()
    cube.prepare({'args': types.SimpleNamespace(shader_ref='WORKTREE'), 'out': str(REPO / '.tmp' / 'cube_r1' / 'sheetout')})
    cube.enabled = [c for c in cube.CONFIGS if c[0] == 'cube']
    cube_cfg = cube.configs({})[0]
    base_tree = G.SourceTree(BASE)
    base_runner = G.AreaRunner(base_tree, gl=cube.runner.gl)
    plans_base, _ = PL.compute_plans(base_tree, REPO / '.tmp' / 'cube_r1')
    opt = {'W': 1920, 'H': 1080, 'stride': 4, 'samples': 256, 'work': str(REPO / '.tmp' / 'cube_r1' / 'sheetwork'), 'cache': args.cache}
    Hg, Wg = 1080 // 4, 1920 // 4
    gj, gi = np.mgrid[0:Hg, 0:Wg]
    fj, fi = gj * 4 + 2, gi * 4 + 2
    for name in args.scenes.split(','):
        sc = T.get_scene(name)
        for pose_id in POSES[name]:
            pidx = [i for i, p in enumerate(sc.poses) if p.id == pose_id][0]
            pose = sc.poses[pidx]
            r = T.pose_task((name, pidx, opt))
            with np.load(r['gb']) as z:
                depth32, norm16, zf, surf = z['depth32'], z['norm16'], z['zf'], z['surf']
            cam = S.Camera(pose.eye, pose.target, 1920, 1080, pose.vfov)
            lights = sc.light_sets[pose.cfg]
            frame = types.SimpleNamespace(scene=sc, pose=pose, cam=cam, lights=lights, depth32=depth32, norm16=norm16)
            screen = base_runner.run(cam, depth32, norm16, lights, plans_base['Default'][len(lights)])[:, fj, fi]
            cubev = cube.visibility(frame, cube_cfg)[:, fj, fi]
            gt = r['gt']
            albedo = [p.albedo for p in sc.prims]
            gt_vis = [L['vis'] * L['cand'] + (~L['cand']) for L in gt['lights']]
            gt_img = SH.shade(gt, gt_vis, albedo)
            panels = []
            texts = []
            for label, prod in (('screen-space (today)', screen), ('cube maps (new)', cubev)):
                L = gt['lights'][0]
                m = L['cand'] & (L['vis'] < 0.98)
                mae = float(np.abs(prod[0] - L['vis'])[m].mean()) if m.any() else float('nan')
                dark = m & (L['vis'] < 0.5)
                leak = float((prod[0][dark] > 0.75).mean()) if dark.any() else float('nan')
                texts.append('%s   mae %.3f   leak %.0f%%' % (label, mae, 100 * leak))
                panels.append((SH.shade(gt, prod, albedo), SH.diff_image(gt, prod, gt_img)))
            k = args.scale
            def up(img):
                im = SH.to_image(img)
                return im.resize((im.width * k, im.height * k), Image.BICUBIC)
            pg = up(gt_img)
            w, h = pg.size
            gap, head = 8, 62
            sheet = Image.new('RGB', (3 * w + 4 * gap, head + 2 * h + 3 * gap), (18, 18, 20))
            dr = ImageDraw.Draw(sheet)
            dr.text((gap, 6), '%s  [%s]  eye (%.1f, %.1f, %.1f)   light (%.1f, %.1f, %.1f)' % (pose.id, sc.description, *pose.eye, *lights[0].pos),
                    fill=(240, 240, 240), font=SH._font(18))
            dr.text((gap, 32), 'top: ground truth | ' + ' | '.join(texts), fill=(220, 220, 170), font=SH._font(14))
            dr.text((gap, head - 16), 'ground truth (ray traced)', fill=(150, 200, 150), font=SH._font(13))
            dr.text((2 * gap + w, head - 16), texts[0], fill=(150, 200, 150), font=SH._font(13))
            dr.text((3 * gap + 2 * w, head - 16), texts[1], fill=(150, 200, 150), font=SH._font(13))
            sheet.paste(pg, (gap, head))
            sheet.paste(up(panels[0][0]), (2 * gap + w, head))
            sheet.paste(up(panels[1][0]), (3 * gap + 2 * w, head))
            y2 = head + h + gap
            sheet.paste(up(gt_img * 0.35), (gap, y2))
            sheet.paste(up(panels[0][1]), (2 * gap + w, y2))
            sheet.paste(up(panels[1][1]), (3 * gap + 2 * w, y2))
            dr.text((gap + 6, y2 + 4), 'ground truth, dimmed (context for the error views)', fill=(200, 200, 200), font=SH._font(13))
            dr.text((2 * gap + w + 6, y2 + 4), 'screen-space error: red = leak, blue = false-dark', fill=(255, 200, 200), font=SH._font(13))
            dr.text((3 * gap + 2 * w + 6, y2 + 4), 'cube maps error: red = leak, blue = false-dark', fill=(255, 200, 200), font=SH._font(13))
            path = out / ('%s.png' % pose.id)
            sheet.save(path, compress_level=4)
            print('[sheets] %s  %s | %s' % (path, texts[0], texts[1]), flush=True)
            Path(r['gb']).unlink(missing_ok=True)


if __name__ == '__main__':
    main()
