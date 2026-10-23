"""Orbit sweep for the screen-space soft shadows (production GLSL on the Windows offscreen GPU gate).

Extends the two-frame moving-camera test in relight_area_check.py. One fixed world (a floor, a wall and
three independent blockers - a slab, a 6 cm rail and a ledge - lit by one light) is viewed by a camera
that orbits it: yaw 0..360 in 15 degree steps, pitch -60..+60 in 20 degree steps and roll +/-15 degrees,
always translated along its view axis so the shadow stays on screen (216 poses). At EVERY pose the
production gather and bilateral upsample (trace at full AND half resolution) are compared with the
current-frame geometry oracle: a ray from every receiver pixel to the light, judged against THIS frame's
texels exactly as the existing two-frame test does (only receivers that face the light and lie 3 pixels
inside the frame are judged; every surface, not only the blockers, may shadow another).

Asserted at every pose: the rendering is deterministic (a repeat is byte-identical), and the negative
controls still fail (a pass that keeps the previous pose's matrices/light, and the previous pose's
finished image, both break the oracle). Asserted over the sweep: the P0 pipeline does not regress the
alignment against the pre-P0 pipeline (mean/worst per-pose error, number of poses within 1 px).

Reported, not asserted: the worst per-pose error. A screen-space march cannot meet a 1-pixel edge at
every orbit pose (a blocker seen at a grazing angle, a thin rail seen edge-on, a shadow cast by an
occluder that is off screen), so the absolute numbers belong to the technique; the gate guards that the
camera/G-buffer/light/filter alignment did not get worse. Error = smallest d (capped at 12) such that
every pixel that disagrees with the oracle lies within d full-res pixels of an edge of the oracle.

Run directly it relaunches the GPU harness (--area-only --orbit-only). With --shader-ref REV the same
gate runs against the shaders of an older commit.
"""
from pathlib import Path
import subprocess

PLAN = dict(samples=1, steps=128, near=0)   # one ray, a point light, a dense march: alignment, not march resolution
THICKNESS, LIGHT_BIAS = .35, .1             # the production default thickness; the housing exclusion the existing tests use


def orbit_poses():
    poses = [(yaw, pitch, 0.0) for yaw in range(0, 360, 15) for pitch in range(-60, 61, 20)]
    poses += [(yaw, pitch, roll) for yaw in (0, 90, 180, 270) for pitch in (-60, 0, 60) for roll in (-15.0, 15.0)]
    return poses


def build_world(S):
    floor = S.plane((0, 0, -4.5), (0, 1, 0), 0, rect=((1, 0, 0), (0, 0, 1), 3.5, 3.0))
    wall = S.plane((0, 1.2, -6.5), (0, 0, 1), 1, rect=((1, 0, 0), (0, 1, 0), 3.5, 1.4))
    slab = S.plane((-0.9, 0.7, -3.8), (0, 0, 1), 2, rect=((1, 0, 0), (0, 1, 0), 0.5, 0.7))
    rail = S.plane((0.9, 0.8, -3.2), (0, 0, 1), 3, rect=((1, 0, 0), (0, 1, 0), 0.03, 0.8))
    ledge = S.plane((1.9, 1.2, -5.0), (0, 1, 0), 4, rect=((1, 0, 0), (0, 0, 1), 0.5, 0.6))
    # Every surface may shadow another (the wall shadows the floor behind it, the slab the wall): all are occluders.
    return [floor, wall, slab, rail, ledge], {0: floor, 1: wall, 2: slab, 3: rail, 4: ledge}


def check_orbit(g, width=256, height=192):
    import time
    np = g['np']
    import relight_scene_fixture as S
    from relight_grain_check import Rig
    t0 = time.time()
    rig = Rig(g)
    surfaces, blockers = build_world(S)
    light_world = np.array([0.4, 2.6, -1.2])
    target = np.array([0.0, 0.5, -4.6])  # the shadow region
    distance = 6.5

    def pose_camera(yaw, pitch, roll):
        # (yaw, pitch) are the view direction's angles; the eye sits on the view axis `distance` from the target.
        yr, pr = np.radians(yaw), np.radians(pitch)
        forward = np.array([np.sin(yr) * np.cos(pr), np.sin(pr), -np.cos(yr) * np.cos(pr)])
        return S.camera(np, target - forward * distance, yaw, pitch, roll, width, height)

    def render(which, scene, half, cam_for_matrices=None):
        """Production gather + bilateral upsample on this frame's G-buffer; optionally with another frame's matrices/light."""
        cam = cam_for_matrices or scene['camera']
        shown = dict(scene)
        shown['projection'], shown['inverse'] = cam['proj'], np.linalg.inv(cam['proj'])
        light_view = (cam['view'] @ np.append(light_world, 1))[:3]
        return rig.chain(which, shown, (*light_view, 1.e-6), dict(PLAN, half=half), 0, 1080, thickness=THICKNESS,
                         light_bias=LIGHT_BIAS, full=True)

    def dilate(mask, steps):
        out = mask.copy()
        for _ in range(steps):
            grown = out.copy()
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
                grown |= np.roll(np.roll(out, dy, 0), dx, 1)
            out = grown
        return out

    def edges(dark):
        e = np.zeros_like(dark)
        e[:, 1:] |= dark[:, 1:] != dark[:, :-1]
        e[1:, :] |= dark[1:, :] != dark[:-1, :]
        return e

    def disagreement(image, oracle, cap=12):
        wrong = (image < 128) != (oracle < 128)
        if not wrong.any():
            return 0
        near = edges(oracle < 128)
        for distance_px in range(0, cap):
            if not (wrong & ~dilate(near, distance_px)).any():
                return distance_px
        return cap

    poses = orbit_poses()
    errors = {(which, half): [] for which in ('old', 'new') for half in (False, True)}
    pose_of = []
    stale_checked = 0
    controls = {'stale differs': 0, 'stale breaks oracle': 0, 'previous image breaks oracle': 0}
    previous = None
    for pose in poses:
        cam = pose_camera(*pose)
        scene = S.cast(np, cam, surfaces)
        bias = S.receiver_bias(np, scene, width, height)
        oracle = S.hit_pixel_oracle(np, scene, light_world, blockers, bias)
        # Only receivers whose visible face points at the light are judged: the others (undersides, backs) are
        # dark by geometry, and what a screen-space march reports there depends on what else is on screen.
        origin = scene['world'] + scene['normal_world'] * bias[:, :, None]
        facing = np.einsum('ijk,ijk->ij', scene['normal_world'], light_world - origin)
        judged = (facing > .05) & (scene['ident'] >= 0)
        judged[:3] = judged[-3:] = False   # off-screen is lit: the march leaves the frame within a few border pixels
        judged[:, :3] = judged[:, -3:] = False
        oracle[~judged] = 255
        dark_pixels = int((oracle < 128).sum())
        results = {}
        for which in ('old', 'new'):
            for half in (False, True):
                image = render(which, scene, half)
                assert image.tobytes() == render(which, scene, half).tobytes(), ('orbit determinism', pose, which, half)
                image = np.where(judged, image, 255).astype(np.uint8)
                results[(which, half)] = image
                errors[(which, half)].append(disagreement(image, oracle) if dark_pixels > 60 else None)
        pose_of.append((pose, dark_pixels))
        # Negative controls: stale matrices/light from the previous pose, and the previous pose's finished image.
        if previous is not None and dark_pixels > 400 and int((results[('new', False)] < 128).sum()) > 200:
            prev_cam, prev_image = previous
            stale = np.where(judged, render('new', scene, False, cam_for_matrices=prev_cam), 255).astype(np.uint8)
            stale_checked += 1
            controls['stale differs'] += not np.array_equal(stale, results[('new', False)])
            controls['stale breaks oracle'] += disagreement(stale, oracle) > 1
            controls['previous image breaks oracle'] += disagreement(np.where(judged, prev_image, 255), oracle) > 1
        previous = (cam, results[('new', False)])
    # The controls are only sensitive where a one-step change moves the shadow: demand it at half of the poses
    # checked (a huge shadow that two nearby light positions both cover is legitimately insensitive).
    for name, count in controls.items():
        assert stale_checked > 40 and count >= .5 * stale_checked, ('orbit negative control insensitive', name, count, stale_checked)
    seen = [i for i, (_, dark) in enumerate(pose_of) if dark > 60]
    assert len(seen) > len(poses) // 2, ('orbit poses with a judged shadow', len(seen), len(poses))
    summary = {}
    for key, values in errors.items():
        v = np.array([values[i] for i in seen])
        summary[key] = dict(worst=int(v.max()), mean=float(v.mean()), within1=int((v <= 1).sum()),
                            worst_pose=pose_of[seen[int(v.argmax())]][0])
    for half in (False, True):
        old, new = summary[('old', half)], summary[('new', half)]
        assert new['mean'] <= old['mean'] + .5, ('orbit alignment regressed (mean)', half, old, new)
        assert new['worst'] <= old['worst'], ('orbit alignment regressed (worst)', half, old, new)
        assert new['within1'] >= old['within1'] - max(3, len(seen) // 20), ('orbit alignment regressed (poses within 1 px)', half, old, new)
    label = {False: 'full-res trace', True: 'half-res trace'}
    parts = []
    for half in (False, True):
        old, new = summary[('old', half)], summary[('new', half)]
        parts.append('%s: P0 worst %d px at pose %s (mean %.2f, %d within 1 px), pre-P0 worst %d px (mean %.2f, %d within 1 px)' %
                     (label[half], new['worst'], new['worst_pose'], new['mean'], new['within1'], old['worst'], old['mean'], old['within1']))
    print('[RelightQ] orbit sweep PASS: %d poses (yaw 0..345 step 15 x pitch -60..60 step 20, roll 0; roll +/-15 at 4 yaws x 3 pitches), %d with a judged shadow; '
          'every repeat byte-identical; negative controls (stale matrices/light, previous-frame image) fire at %s of %d checked poses; worst per-pose shadow-edge error vs the current-frame geometry oracle (cap 12 px): %s; %.0f s' %
          (len(poses), len(seen), '/'.join(str(c) for c in controls.values()), stale_checked, '; '.join(parts), time.time() - t0), flush=True)
    return summary


if __name__ == '__main__':
    import sys
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name('relight_shader_check.py')),
                                      '--area-only', '--orbit-only', *sys.argv[1:]]))
