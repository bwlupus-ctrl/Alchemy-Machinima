"""Phase 0 / P0.4 decision study (CPU only, numpy). Terminator-band radiance study for P0.4 (skip below-horizon disk rays and renormalize).
Truth: Monte Carlo (deterministic quasi-random) integral of the unoccluded finite DISK light over the
hemisphere above the GEOMETRIC horizon, weighted by the SHADING normal's cosine. Candidates multiply the
shading term by the visibility the gather would report: HEAD = fraction of disk rays above the geometric
horizon (the receiver's own surface blocks the rest); (a) = 1 (skip and renormalize)."""
import numpy as np

rng = np.random.default_rng(1234)
N = 65536
# R2 points on the unit disk (area-uniform)
i = np.arange(1, N + 1)
u = (0.5 + 0.7548776662466927 * i) % 1
v = (0.5 + 0.5698402909980532 * i) % 1
rad, ang = np.sqrt(u), 2 * np.pi * v


def relight_sphere(c, s2):
    c = np.clip(c, -1, 1)
    out = np.empty_like(c)
    for k in range(c.size):
        ck = c.flat[k]
        if s2 < 1e-6 or ck * ck > s2:
            out.flat[k] = np.pi * s2 * max(ck, 0)
            continue
        st = np.sqrt(max(1 - ck * ck, 1e-8))
        x = np.sqrt(max(1 / max(s2, 1e-8) - 1, 0))
        y = np.clip(-x * ck / st, -1, 1)
        sy = np.sqrt(max(1 - y * y, 0))
        out.flat[k] = np.clip((ck * np.arccos(y) - x * st * sy) * s2 + np.arctan2(st * sy, x), 0, np.pi)
    return out


def relight_ratio(n, lv, radius):
    d2 = np.sum(lv * lv, -1)
    s2 = float(np.minimum(1, radius ** 2 / np.maximum(d2, 1e-12)).ravel()[0])
    unit = lv / np.sqrt(d2)[..., None]
    c = np.sum(n * unit, -1)
    res = np.where(c >= np.sqrt(s2), c, relight_sphere(c, s2) / (np.pi * np.maximum(s2, 1e-20)))
    return res


def band(distance, radius, tilt_deg, mode, azimuth=0.7):
    # Sphere receiver: surface points parameterised by the angle between the normal and the light centre.
    sigma = np.arcsin(min(radius / distance, 1))
    thetas = np.linspace(np.pi / 2 - sigma - 0.05, np.pi / 2 + sigma + 0.05 + 0.25, 241)  # cos_c from +sigma.. to -sigma
    results = {'head': [], 'skip': []}
    for th in thetas:
        n_geo = np.array([np.sin(th), 0, np.cos(th)])  # light centre along +z
        lc = np.array([0, 0, distance])
        # disk perpendicular to the centre direction
        pts = np.stack([radius * rad * np.cos(ang), radius * rad * np.sin(ang), np.full(N, distance)], -1)
        omega = pts / np.linalg.norm(pts, axis=-1, keepdims=True)
        # shading normal: tilted away by tilt_deg in a fixed azimuth
        t = np.radians(tilt_deg)
        e1 = np.array([np.cos(th), 0, -np.sin(th)])
        e2 = np.array([0, 1, 0])
        n_s = np.cos(t) * n_geo + np.sin(t) * (np.cos(azimuth) * e1 + np.sin(azimuth) * e2)
        above = omega @ n_geo > 0
        truth = np.mean(np.where(above, np.maximum(omega @ n_s, 0), 0))
        nu = np.mean(above)
        if mode == 'off':
            shade = max(n_s @ (lc / distance), 0)
        else:
            shade = float(relight_ratio(n_s[None], lc[None], radius)[0])
        results['head'].append((shade * nu, truth, nu))
        results['skip'].append((shade * (1.0 if nu > 0 else 0.0), truth, 1.0))
    out = {}
    for key, rows in results.items():
        a = np.array(rows)
        out[key] = np.sqrt(np.mean((a[:, 0] - a[:, 1]) ** 2)) * 255
    return out


def horizon_study(report=False):
    """Compare HEAD's below-horizon-rays-blocked visibility with P0.4(a) (skip and renormalise) against
    the lit-radiance reference. Returns (wins, total, worst_head, worst_skip, rows). The decision rule
    of the plan: adopt (a) only if it lowers the terminator-band radiance error with Light shape OFF
    and ON AND on normal-mapped receivers. It does not: with the shading normal tilted toward the
    light the dark side leaks. P0.4 is therefore NOT adopted and the gather keeps HEAD's arithmetic."""
    rows = []
    for distance in (4.0, 8.0):
        for radius in (0.1, 0.3, 1.0):
            for mode in ('off', 'on'):
                for label, tilt, az in (('flat', 0, 0.0), ('tilt toward', 25, np.pi + 0.7),
                                        ('tilt away', 25, 0.7), ('tilt side', 25, np.pi / 2)):
                    r = band(distance, radius, tilt, mode, azimuth=az)
                    rows.append((distance, radius, mode, label, r['head'], r['skip']))
                    if report:
                        print('d=%.0f r=%.1f shape %-3s %-11s: HEAD %.2f  skip %.2f  %s' %
                              (distance, radius, mode, label, r['head'], r['skip'],
                               'skip better' if r['skip'] < r['head'] else 'HEAD better'))
    wins = sum(1 for row in rows if row[5] < row[4])
    return wins, len(rows), max(row[4] for row in rows), max(row[5] for row in rows), rows


if __name__ == '__main__':
    wins, total, worst_head, worst_skip, _ = horizon_study(True)
    print('skip wins', wins, 'of', total, '; worst HEAD %.2f/255, worst skip %.2f/255' % (worst_head, worst_skip))
